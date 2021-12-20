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
#define EXPOSE_IMAGE_UTILS
#include "skybox.h"
#include "model.h"
#include "renderer.h"
#include "util.h"
#include "menus/menus.h"
namespace vkrollercoaster {
    static struct {
        ref<vertex_buffer> vertices;
        ref<index_buffer> indices;
    } skybox_data;

    void skybox::init() {
        // load the mesh
        auto source = ref<model_source>::create("assets/models/cube.gltf");
        auto _model = ref<model>::create(source);

        // vertices
        const auto& vertices = _model->get_vertices();
        std::vector<glm::vec3> positions;
        for (const auto& _vertex : vertices) {
            positions.push_back(_vertex.position);
        }
        skybox_data.vertices = ref<vertex_buffer>::create(vertices);

        // indices
        const auto& indices = _model->get_indices();
        skybox_data.indices = ref<index_buffer>::create(indices);
    }

    void skybox::shutdown() {
        skybox_data.vertices.reset();
        skybox_data.indices.reset();
    }

    struct skybox_data_t {
        float exposure, gamma;
    };

    skybox::skybox(ref<image_cube> skybox_texture) {
        auto _shader = shader_library::get("skybox");

        // define pipeline layout
        pipeline_spec spec;
        spec.enable_blending = false;
        spec.enable_depth_testing = true;
        spec.input_layout.stride = sizeof(glm::vec3);
        spec.input_layout.attributes = { { vertex_attribute_type::VEC3, 0 } };

        // create pipeline
        ref<render_target> rendertarget = viewport::get_instance()->get_framebuffer();
        this->m_pipeline = ref<pipeline>::create(rendertarget, _shader, spec);

        // create uniform buffer for skybox.hlsl
        auto& reflection_data = _shader->get_reflection_data();
        uint32_t set, binding;
        if (!reflection_data.find_resource("skybox_data", set, binding)) {
            throw std::runtime_error("could not find the skybox uniform buffer!");
        }
        this->m_uniform_buffer = uniform_buffer::from_shader_data(_shader, set, binding);

        // set initial exposure and gamma
        skybox_data_t initial_data;
        initial_data.exposure = 4.5f;
        initial_data.gamma = 2.2f;
        this->m_uniform_buffer->set_data(initial_data);

        // create texture for rendering
        this->m_skybox = ref<texture>::create(skybox_texture);

        // bind objects to pipeline
        this->m_skybox->bind(this->m_pipeline, 1, 0);
        this->m_uniform_buffer->bind(this->m_pipeline);
        renderer::get_camera_buffer()->bind(this->m_pipeline);

        // create pbr textures
        this->create_irradiance_map();
    }

    void skybox::render(ref<command_buffer> cmdbuffer, bool bind_pipeline) {
        auto target = this->m_pipeline->get_render_target();

        // mesh data
        skybox_data.vertices->bind(cmdbuffer);
        skybox_data.indices->bind(cmdbuffer);

        if (bind_pipeline) {
            // set scissor
            VkRect2D scissor = this->m_pipeline->get_scissor();
            vkCmdSetScissor(cmdbuffer->get(), 0, 1, &scissor);

            // set viewport
            VkViewport viewport = this->m_pipeline->get_viewport();
            viewport.y = (float)target->get_extent().height - viewport.y;
            viewport.height *= -1.f;
            vkCmdSetViewport(cmdbuffer->get(), 0, 1, &viewport);

            // finally, bind the damn pipeline
            this->m_pipeline->bind(cmdbuffer);
        }

        // draw
        vkCmdDrawIndexed(cmdbuffer->get(), skybox_data.indices->get_index_count(), 1, 0, 0, 0);
    }

    void skybox::create_irradiance_map() {
        static constexpr VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
        static constexpr uint32_t size = 64;

        // create final irradiance map
        auto irradiance_map = ref<image_cube>::create(
            format, size, size, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);
        static constexpr VkImageLayout irradiance_transfer_image_layout =
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        irradiance_map->transition(irradiance_transfer_image_layout);

        // create color attachment for rendering framebuffer
        auto attachment = ref<image2d>::create(format, size, size,
                                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                               VK_IMAGE_ASPECT_COLOR_BIT);
        attachment->transition(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // create framebuffer to render to
        framebuffer_spec fb_spec;
        fb_spec.width = size;
        fb_spec.height = size;
        fb_spec.provided_attachments[attachment_type::color] = attachment;
        auto fb = ref<framebuffer>::create(fb_spec);

        // create pipeline for rendering
        auto _shader = shader_library::get("irradiance_map");
        pipeline_spec _pipeline_spec;
        _pipeline_spec.input_layout.stride = sizeof(glm::vec3);
        _pipeline_spec.input_layout.attributes = { { vertex_attribute_type::VEC3, 0 } };
        auto _pipeline = ref<pipeline>::create(fb, _shader, _pipeline_spec);

        // create sampling deltas uniform buffer
        struct {
            float delta_phi, delta_theta;
        } sampling_deltas;
        sampling_deltas.delta_phi = (float)M_PI / 90.f;
        sampling_deltas.delta_theta = (float)M_PI / 128.f;
        uint32_t set, binding;
        auto& reflection_data = _shader->get_reflection_data();
        if (!reflection_data.find_resource("sampling_deltas", set, binding)) {
            throw std::runtime_error("could not find sampling deltas buffer");
        }
        auto sampling_deltas_ubo = uniform_buffer::from_shader_data(_shader, set, binding);
        sampling_deltas_ubo->set_data(sampling_deltas);

        // bind skybox texture and uniform buffer to pipeline
        this->m_skybox->bind(_pipeline, "environment_texture");
        sampling_deltas_ubo->bind(_pipeline);

        // mvp matrices (see assets/shaders/base/filter_cube.hlsl)
        std::vector<glm::mat4> matrices = {
            // positive x
            glm::rotate(glm::rotate(glm::mat4(1.f), glm::radians(90.f), glm::vec3(0.f, 1.f, 0.f)),
                        glm::radians(180.f), glm::vec3(1.f, 0.f, 0.f)),

            // negative x
            glm::rotate(glm::rotate(glm::mat4(1.f), glm::radians(-90.f), glm::vec3(0.f, 1.f, 0.f)),
                        glm::radians(180.f), glm::vec3(1.f, 0.f, 0.f)),

            // positive y
            glm::rotate(glm::mat4(1.f), glm::radians(-90.f), glm::vec3(1.f, 0.f, 0.f)),

            // negative y
            glm::rotate(glm::mat4(1.f), glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f)),

            // positive z
            glm::rotate(glm::mat4(1.f), glm::radians(180.f), glm::vec3(1.f, 0.f, 0.f)),

            // negative z
            glm::rotate(glm::mat4(1.f), glm::radians(180.f), glm::vec3(0.f, 0.f, 1.f)),
        };

        {
            VkImage attachment_image = attachment->get_image();
            VkImage irradiance_image = irradiance_map->get_image();

            auto cmdbuffer = renderer::create_single_time_command_buffer();
            cmdbuffer->begin();

            // set scissor
            VkRect2D scissor = _pipeline->get_scissor();
            vkCmdSetScissor(cmdbuffer->get(), 0, 1, &scissor);

            // set viewport
            VkViewport viewport = _pipeline->get_viewport();
            viewport.y = (float)fb->get_extent().height - viewport.y;
            viewport.height *= -1.f;
            vkCmdSetViewport(cmdbuffer->get(), 0, 1, &viewport);

            for (uint32_t face = 0; face < image_cube::cube_face_count; face++) {
                cmdbuffer->begin_render_pass(fb, glm::vec4(0.f));
                _pipeline->bind(cmdbuffer);

                glm::mat4 mvp =
                    glm::perspective(glm::radians(90.f), 1.f, 0.1f, 512.f) * matrices[face];
                vkCmdPushConstants(cmdbuffer->get(), _pipeline->get_layout(),
                                   VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mvp);

                this->render(cmdbuffer, false);
                cmdbuffer->end_render_pass();

                static constexpr VkImageLayout transfer_image_layout =
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                VkImageLayout render_layout = attachment->get_layout();
                transition_image_layout(attachment_image, render_layout, transfer_image_layout,
                                        attachment->get_image_aspect(), 1, cmdbuffer);

                VkImageCopy region;
                util::zero(region);

                region.srcSubresource.aspectMask = attachment->get_image_aspect();
                region.srcSubresource.baseArrayLayer = 0;
                region.srcSubresource.mipLevel = 0;
                region.srcSubresource.layerCount = 1;
                region.srcOffset = { 0, 0, 0 };

                region.dstSubresource.aspectMask = irradiance_map->get_image_aspect();
                region.dstSubresource.baseArrayLayer = face;
                region.dstSubresource.mipLevel = 0;
                region.dstSubresource.layerCount = 1;
                region.dstOffset = { 0, 0, 0 };

                region.extent.width = size;
                region.extent.height = size;
                region.extent.depth = 1;

                vkCmdCopyImage(cmdbuffer->get(), attachment_image, transfer_image_layout,
                               irradiance_image, irradiance_map->get_layout(), 1, &region);
                transition_image_layout(attachment_image, transfer_image_layout, render_layout,
                                        attachment->get_image_aspect(), 1, cmdbuffer);
            }

            static constexpr VkImageLayout final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            transition_image_layout(irradiance_image, irradiance_transfer_image_layout,
                                    final_layout, irradiance_map->get_image_aspect(),
                                    image_cube::cube_face_count, cmdbuffer);

            cmdbuffer->end();
            cmdbuffer->submit();
            cmdbuffer->wait();

            irradiance_map->set_layout(final_layout);
        }

        this->m_irradiance_map = ref<texture>::create(irradiance_map);
    }
} // namespace vkrollercoaster