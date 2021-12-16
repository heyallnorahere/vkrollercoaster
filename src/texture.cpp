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
#include "texture.h"
#include "renderer.h"
#include "util.h"
#include "imgui_controller.h"
#include <backends/imgui_impl_vulkan.h>
namespace vkrollercoaster {
    texture::texture(ref<image> _image, bool transition_layout) {
        renderer::add_ref();
        this->m_image = _image;

        if (transition_layout) {
            static constexpr VkImageLayout ideal_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            if (this->m_image->get_layout() != ideal_layout) {
                this->m_image->transition(ideal_layout);
            }
        }

        this->m_image->m_dependents.insert(this);
        this->create_sampler();
    }
    texture::~texture() {
        for (auto _pipeline : this->m_bound_pipelines) {
            std::vector<pipeline::texture_binding_desc> bindings;
            for (const auto& [binding, tex] : _pipeline->m_bound_textures) {
                if (tex == this) {
                    bindings.push_back(binding);
                }
            }
            for (const auto& binding : bindings) {
                _pipeline->m_bound_textures.erase(binding);
            }
        }
        VkDevice device = renderer::get_device();
        vkDestroySampler(device, this->m_sampler, nullptr);
        if (this->m_imgui_id) {
            ImGui_ImplVulkan_RemoveTexture(this->m_imgui_id);
            imgui_controller::remove_dependent();
        }
        this->m_image->m_dependents.erase(this);
        renderer::remove_ref();
    }
    void texture::bind(ref<pipeline> _pipeline, uint32_t set, uint32_t binding, uint32_t slot) {
        auto _shader = _pipeline->get_shader();
        auto& reflection_data = _shader->get_reflection_data();
        const auto& resource = reflection_data.resources[set][binding];
        std::string binding_string =
            "binding " + std::to_string(set) + "." + std::to_string(binding);
        if (resource.resource_type != shader_resource_type::sampledimage) {
            throw std::runtime_error(binding_string + " is not a sampled image!");
        }
        const auto& resource_type = reflection_data.types[resource.type];
        if (slot >= resource_type.array_size) {
            throw std::runtime_error(
                "index " + std::to_string(slot) + " is out of the array range (" +
                std::to_string(resource_type.array_size) + ") of " + binding_string + "!");
        }
        auto& sets = _pipeline->m_descriptor_sets;
        if (sets.find(set) == sets.end()) {
            throw std::runtime_error("set " + std::to_string(set) +
                                     " does not exist on the given pipeline!");
        }
        VkDescriptorImageInfo image_info;
        util::zero(image_info);
        image_info.imageLayout = this->m_image->m_layout;
        image_info.imageView = this->m_image->m_view;
        image_info.sampler = this->m_sampler;
        std::vector<VkWriteDescriptorSet> writes;
        for (VkDescriptorSet current_set : sets[set].sets) {
            VkWriteDescriptorSet write;
            util::zero(write);
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = current_set;
            write.dstBinding = binding;
            write.dstArrayElement = slot;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &image_info;
            writes.push_back(write);
        }
        VkDevice device = renderer::get_device();
        vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
        if (this->m_bound_pipelines.find(_pipeline.raw()) == this->m_bound_pipelines.end()) {
            this->m_bound_pipelines.insert(_pipeline.raw());
        }
        pipeline::texture_binding_desc binding_desc;
        binding_desc.set = set;
        binding_desc.binding = binding;
        binding_desc.slot = slot;
        if (_pipeline->m_bound_textures[binding_desc] != this) {
            _pipeline->m_bound_textures[binding_desc] = this;
        }
    }
    void texture::bind(ref<pipeline> _pipeline, const std::string& name, uint32_t slot) {
        auto _shader = _pipeline->get_shader();
        auto& reflection_data = _shader->get_reflection_data();
        uint32_t set, binding;
        if (reflection_data.find_resource(name, set, binding)) {
            this->bind(_pipeline, set, binding, slot);
        } else {
            throw std::runtime_error("the specified resource was not found!");
        }
    }
    ImTextureID texture::get_imgui_id() {
        if (!this->m_imgui_id) {
            imgui_controller::add_dependent();
            this->m_imgui_id = ImGui_ImplVulkan_AddTexture(this->m_sampler, this->m_image->m_view,
                                                           this->m_image->m_layout);
        }
        return this->m_imgui_id;
    }
    void texture::create_sampler() {
        VkPhysicalDevice physical_device = renderer::get_physical_device();
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physical_device, &properties);
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(physical_device, &features);
        VkSamplerCreateInfo create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        // todo: sampler settings
        create_info.magFilter = VK_FILTER_LINEAR;
        create_info.minFilter = VK_FILTER_LINEAR;
        create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        if (features.samplerAnisotropy) {
            create_info.anisotropyEnable = true;
            create_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        } else {
            create_info.anisotropyEnable = false;
            create_info.maxAnisotropy = 1.f;
        }
        create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        create_info.unnormalizedCoordinates = false;
        create_info.compareEnable = false;
        create_info.compareOp = VK_COMPARE_OP_ALWAYS;
        create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        create_info.mipLodBias = 0.f;
        create_info.minLod = 0.f;
        create_info.maxLod = 0.f;
        VkDevice device = renderer::get_device();
        if (vkCreateSampler(device, &create_info, nullptr, &this->m_sampler) != VK_SUCCESS) {
            throw std::runtime_error("could not create sampler!");
        }
    }
    void texture::update_imgui_texture() {
        if (this->m_imgui_id) {
            ImGui_ImplVulkan_UpdateTextureInfo(this->m_imgui_id, this->m_sampler,
                                               this->m_image->m_view, this->m_image->m_layout);
        }
    }
} // namespace vkrollercoaster