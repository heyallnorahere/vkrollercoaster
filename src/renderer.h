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

#pragma once
#include "window.h"
#include "command_buffer.h"
#include "scene.h"
#include "texture.h"
#include "buffers.h"
#include "pipeline.h"
namespace vkrollercoaster {
#ifdef EXPOSE_RENDERER_INTERNALS
    struct queue_family_indices {
        std::optional<uint32_t> graphics_family, compute_family;
        bool complete() const {
            const std::vector<bool> families_found = {
                this->graphics_family.has_value(),
                this->compute_family.has_value(),
            };
            for (bool found : families_found) {
                if (!found) {
                    return false;
                }
            }
            return true;
        }
        std::set<uint32_t> create_set() const {
            return { *this->graphics_family, *this->compute_family };
        }
    };
    struct sync_objects {
        VkSemaphore image_available_semaphore = nullptr;
        VkSemaphore render_finished_semaphore = nullptr;
        VkFence fence = nullptr;
    };
    struct submitted_render_call {
        ref<pipeline> _pipeline;
        ref<vertex_buffer> vbo;
        ref<index_buffer> ibo;
    };
    struct internal_cmdbuffer_data {
        std::vector<submitted_render_call> submitted_calls;
    };
#endif
    class renderer {
    public:
        renderer() = delete;

        static void add_layer(const std::string& name);
        static void add_instance_extension(const std::string& name);
        static void add_device_extension(const std::string& name);

        static void init(uint32_t vulkan_version = VK_API_VERSION_1_0);
        static void shutdown();
        static void new_frame();

        static void render_entity(ref<command_buffer> cmdbuffer, entity to_render);
        static void render_track(ref<command_buffer> cmdbuffer, entity track);

        static void add_ref();
        static void remove_ref();

        static ref<command_buffer> create_render_command_buffer();
        static ref<command_buffer> create_single_time_command_buffer();

        static uint32_t get_vulkan_version();
        static VkInstance get_instance();
        static VkPhysicalDevice get_physical_device();
        static VkDevice get_device();
        static VkQueue get_graphics_queue();
        static VkQueue get_compute_queue();
        static VkDescriptorPool get_descriptor_pool();
        static ref<texture> get_white_texture();
        static ref<uniform_buffer> get_camera_buffer();

        static void update_camera_buffer(ref<scene> _scene, ref<window> _window);

        static void expand_vulkan_version(uint32_t version, uint32_t& major, uint32_t& minor,
                                          uint32_t& patch);

#ifdef EXPOSE_RENDERER_INTERNALS
        static queue_family_indices find_queue_families(VkPhysicalDevice device);
        static const sync_objects& get_sync_objects(size_t frame_index);
        static size_t get_current_frame();

        static constexpr size_t max_frame_count = 2;
#endif
    };
} // namespace vkrollercoaster