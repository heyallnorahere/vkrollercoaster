/*
   Copyright 2021 Nora Beda and contributors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "pch.h"
#include "pipeline.h"
#include "renderer.h"
#include "util.h"
#include "buffers.h"
#include "texture.h"
#include "material.h"
#include "swapchain.h"
namespace vkrollercoaster {
    pipeline::pipeline(ref<render_target> target, ref<shader> _shader, const pipeline_spec& spec) {
        this->m_material = nullptr;
        this->m_render_target = target;
        this->m_shader = _shader;
        this->m_spec = spec;
        renderer::add_ref();
        this->create_descriptor_sets();
        this->create_pipeline();
        this->m_shader->m_dependents.insert(this);
        auto destroy = [this]() mutable { this->destroy_pipeline(); };
        auto recreate = [this]() mutable { this->create_pipeline(); };
        this->m_render_target->add_reload_callbacks(this, destroy, recreate);
    }
    pipeline::~pipeline() {
        if (this->m_material) {
            this->m_material->m_created_pipelines.erase(this);
        }
        for (const auto& [set, bindings] : this->m_bound_buffers) {
            for (const auto& [binding, data] : bindings) {
                switch (data.type) {
                case buffer_type::ubo: {
                    auto ubo = (uniform_buffer*)data.object;
                    ubo->m_bound_pipelines.erase(this);
                } break;
                }
            }
        }
        for (const auto& [binding, tex] : this->m_bound_textures) {
            tex->m_bound_pipelines.erase(this);
        }
        this->m_render_target->remove_reload_callbacks(this);
        this->m_shader->m_dependents.erase(this);
        this->destroy_pipeline();
        this->destroy_descriptor_sets();
        renderer::remove_ref();
    }
    void pipeline::bind(ref<command_buffer> cmdbuffer) {
        size_t set_index = 0;
        if (this->m_render_target->get_render_target_type() == render_target_type::swapchain) {
            ref<swapchain> swap_chain = this->m_render_target.as<swapchain>();
            set_index = swap_chain->get_current_image();
        }
        VkCommandBuffer vk_cmdbuffer = cmdbuffer->get();
        vkCmdBindPipeline(vk_cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->m_pipeline);
        for (const auto& [set, data] : this->m_descriptor_sets) {
            vkCmdBindDescriptorSets(vk_cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->m_layout,
                                    set, 1, &data.sets[set_index], 0, nullptr);
        }
    }
    void pipeline::reload(bool descriptor_sets) {
        this->destroy_pipeline();
        if (descriptor_sets) {
            this->destroy_descriptor_sets();
            this->create_descriptor_sets();
            this->rebind_objects();
        }
        this->create_pipeline();
    }
    void pipeline::create_descriptor_sets() {
        this->m_push_constant_ranges.clear();
        const auto& reflection_data = this->m_shader->get_reflection_data();
        for (const auto& push_constant : reflection_data.push_constant_buffers) {
            VkPushConstantRange range;
            util::zero(range);
            range.stageFlags = shader::get_stage_flags(push_constant.stage);
            range.offset = 0;
            range.size = reflection_data.types[push_constant.type].size;
            this->m_push_constant_ranges.push_back(range);
        }
        if (reflection_data.resources.empty()) {
            return;
        }
        std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> bindings;
        for (const auto& [set, resources] : reflection_data.resources) {
            for (const auto& [binding, data] : resources) {
                VkDescriptorSetLayoutBinding set_binding;
                util::zero(set_binding);
                set_binding.binding = binding;
                set_binding.stageFlags = shader::get_stage_flags(data.stage);
                const auto& resource_type = reflection_data.types[data.type];
                set_binding.descriptorCount = resource_type.array_size;
                switch (data.resource_type) {
                case shader_resource_type::uniformbuffer:
                    set_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    break;
                case shader_resource_type::storagebuffer:
                    set_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    break;
                case shader_resource_type::sampledimage:
                    set_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    break;
                default:
                    throw std::runtime_error("invalid resource type!");
                }
                bindings[set].push_back(set_binding);
            }
        }
        size_t set_count = 1;
        if (this->m_render_target->get_render_target_type() == render_target_type::swapchain) {
            ref<swapchain> swap_chain = this->m_render_target.as<swapchain>();
            set_count = swap_chain->get_swapchain_images().size();
        }
        VkDevice device = renderer::get_device();
        VkDescriptorPool descriptor_pool = renderer::get_descriptor_pool();
        for (const auto& [set, set_bindings] : bindings) {
            VkDescriptorSetLayoutCreateInfo create_info;
            util::zero(create_info);
            create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            create_info.bindingCount = set_bindings.size();
            create_info.pBindings = set_bindings.data();
            VkDescriptorSetLayout layout;
            if (vkCreateDescriptorSetLayout(device, &create_info, nullptr, &layout) != VK_SUCCESS) {
                throw std::runtime_error("could not create descriptor set layout!");
            }
            auto& set_data = this->m_descriptor_sets[set];
            set_data.layout = layout;
            set_data.sets.resize(set_count);
            std::vector<VkDescriptorSetLayout> layouts(set_count, layout);
            VkDescriptorSetAllocateInfo alloc_info;
            util::zero(alloc_info);
            alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc_info.descriptorPool = descriptor_pool;
            alloc_info.descriptorSetCount = layouts.size();
            alloc_info.pSetLayouts = layouts.data();
            if (vkAllocateDescriptorSets(device, &alloc_info, set_data.sets.data()) != VK_SUCCESS) {
                throw std::runtime_error("could not allocate descriptor sets!");
            }
        }
    }
    void pipeline::create_pipeline() {
        VkDevice device = renderer::get_device();
        VkExtent2D extent = this->m_render_target->get_extent();
        VkPipelineVertexInputStateCreateInfo vertex_input_info;
        util::zero(vertex_input_info);
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        std::vector<VkVertexInputAttributeDescription> attributes;
        VkVertexInputBindingDescription binding;
        util::zero(binding);
        binding.binding = 0;
        binding.stride = this->m_spec.input_layout.stride;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        vertex_input_info.vertexBindingDescriptionCount = 1;
        vertex_input_info.pVertexBindingDescriptions = &binding;
        for (uint32_t i = 0; i < this->m_spec.input_layout.attributes.size(); i++) {
            const auto& attribute = this->m_spec.input_layout.attributes[i];
            auto& attribute_desc = attributes.emplace_back();
            util::zero(attribute_desc);
            attribute_desc.binding = 0;
            attribute_desc.location = i;
            attribute_desc.offset = attribute.offset;
            switch (attribute.type) {
            case vertex_attribute_type::FLOAT:
                attribute_desc.format = VK_FORMAT_R32_SFLOAT;
                break;
            case vertex_attribute_type::INT:
                attribute_desc.format = VK_FORMAT_R32_SINT;
                break;
            case vertex_attribute_type::VEC2:
                attribute_desc.format = VK_FORMAT_R32G32_SFLOAT;
                break;
            case vertex_attribute_type::IVEC2:
                attribute_desc.format = VK_FORMAT_R32G32_SINT;
                break;
            case vertex_attribute_type::VEC3:
                attribute_desc.format = VK_FORMAT_R32G32B32_SFLOAT;
                break;
            case vertex_attribute_type::IVEC3:
                attribute_desc.format = VK_FORMAT_R32G32B32_SINT;
                break;
            case vertex_attribute_type::VEC4:
                attribute_desc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
                break;
            case vertex_attribute_type::IVEC4:
                attribute_desc.format = VK_FORMAT_R32G32B32A32_SINT;
                break;
            case vertex_attribute_type::BOOLEAN:
                attribute_desc.format = VK_FORMAT_R8_UINT;
                break;
            default:
                throw std::runtime_error("invalid vertex attribute type!");
            }
        }
        if (!attributes.empty()) {
            vertex_input_info.vertexAttributeDescriptionCount = attributes.size();
            vertex_input_info.pVertexAttributeDescriptions = attributes.data();
        }
        VkPipelineInputAssemblyStateCreateInfo input_assembly;
        util::zero(input_assembly);
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = false;
        util::zero(this->m_viewport);
        this->m_viewport.x = this->m_viewport.y = 0.f;
        this->m_viewport.width = (float)extent.width;
        this->m_viewport.height = (float)extent.height;
        this->m_viewport.minDepth = 0.f;
        this->m_viewport.maxDepth = 1.f;
        util::zero(this->m_scissor);
        this->m_scissor.extent = extent;
        VkPipelineViewportStateCreateInfo viewport_state;
        util::zero(viewport_state);
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &this->m_viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &this->m_scissor;
        VkPipelineRasterizationStateCreateInfo rasterizer;
        util::zero(rasterizer);
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = false;
        rasterizer.rasterizerDiscardEnable = false;
        switch (this->m_spec.polygon_mode) {
        case pipeline_polygon_mode::fill:
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            break;
        case pipeline_polygon_mode::wireframe:
            rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
            break;
        default:
            throw std::runtime_error("invalid polygon mode!");
        }
        rasterizer.lineWidth = 1.f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        switch (this->m_spec.front_face) {
        case pipeline_front_face::clockwise:
            rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
            break;
        case pipeline_front_face::counter_clockwise:
            rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            break;
        default:
            throw std::runtime_error("invalid front face!");
        }
        rasterizer.depthBiasEnable = false;
        VkPipelineMultisampleStateCreateInfo multisampling;
        util::zero(multisampling);
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = false;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depth_stencil;
        util::zero(depth_stencil);
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        if (this->m_spec.enable_depth_testing) {
            depth_stencil.depthTestEnable = true;
            depth_stencil.depthWriteEnable = true;
            depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
        }
        VkPipelineColorBlendAttachmentState color_blend_attachment;
        util::zero(color_blend_attachment);
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                VK_COLOR_COMPONENT_G_BIT |
                                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        if (this->m_spec.enable_blending) {
            color_blend_attachment.blendEnable = true;
            color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
            color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }
        VkPipelineColorBlendStateCreateInfo color_blending;
        util::zero(color_blending);
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = false;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;
        std::vector<VkDynamicState> dynamic_states = { VK_DYNAMIC_STATE_VIEWPORT,
                                                       VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic_state;
        util::zero(dynamic_state);
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = dynamic_states.size();
        dynamic_state.pDynamicStates = dynamic_states.data();
        VkPipelineLayoutCreateInfo layout_create_info;
        util::zero(layout_create_info);
        layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        std::vector<VkDescriptorSetLayout> set_layouts;
        for (const auto& [_, set] : this->m_descriptor_sets) {
            set_layouts.push_back(set.layout);
        }
        if (!set_layouts.empty()) {
            layout_create_info.setLayoutCount = set_layouts.size();
            layout_create_info.pSetLayouts = set_layouts.data();
        }
        if (!this->m_push_constant_ranges.empty()) {
            layout_create_info.pushConstantRangeCount = this->m_push_constant_ranges.size();
            layout_create_info.pPushConstantRanges = this->m_push_constant_ranges.data();
        }
        if (vkCreatePipelineLayout(device, &layout_create_info, nullptr, &this->m_layout) !=
            VK_SUCCESS) {
            throw std::runtime_error("could not create pipeline layout!");
        }
        const auto& stages = this->m_shader->get_pipeline_info();
        VkGraphicsPipelineCreateInfo create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        create_info.stageCount = stages.size();
        create_info.pStages = stages.data();
        create_info.pVertexInputState = &vertex_input_info;
        create_info.pInputAssemblyState = &input_assembly;
        create_info.pViewportState = &viewport_state;
        create_info.pRasterizationState = &rasterizer;
        create_info.pMultisampleState = &multisampling;
        create_info.pDepthStencilState = &depth_stencil;
        create_info.pColorBlendState = &color_blending;
        create_info.pDynamicState = &dynamic_state;
        create_info.layout = this->m_layout;
        create_info.renderPass = this->m_render_target->get_render_pass();
        create_info.subpass = 0;
        create_info.basePipelineHandle = nullptr;
        create_info.basePipelineIndex = -1;
        if (vkCreateGraphicsPipelines(device, nullptr, 1, &create_info, nullptr,
                                      &this->m_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("could not create pipeline!");
        }
    }
    void pipeline::destroy_pipeline() {
        VkDevice device = renderer::get_device();
        vkDestroyPipeline(device, this->m_pipeline, nullptr);
        vkDestroyPipelineLayout(device, this->m_layout, nullptr);
    }
    void pipeline::destroy_descriptor_sets() {
        VkDevice device = renderer::get_device();
        VkDescriptorPool descriptor_pool = renderer::get_descriptor_pool();
        for (const auto& [_, set] : this->m_descriptor_sets) {
            vkFreeDescriptorSets(device, descriptor_pool, set.sets.size(), set.sets.data());
            vkDestroyDescriptorSetLayout(device, set.layout, nullptr);
        }
    }
    void pipeline::rebind_objects() {
        for (const auto& [set, bindings] : this->m_bound_buffers) {
            for (const auto& [binding, data] : bindings) {
                switch (data.type) {
                case buffer_type::ubo: {
                    auto ubo = (uniform_buffer*)data.object;
                    ubo->bind(this);
                } break;
                default:
                    throw std::runtime_error("invalid buffer type!");
                }
            }
        }
        for (const auto& [binding, tex] : this->m_bound_textures) {
            tex->bind(this, binding.set, binding.binding, binding.slot);
        }
    }
} // namespace vkrollercoaster