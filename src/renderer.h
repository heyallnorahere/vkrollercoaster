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
namespace vkrollercoaster {
#ifdef EXPOSE_RENDERER_INTERNALS
    struct swapchain_support_details {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
    };
    struct queue_family_indices {
        std::optional<uint32_t> graphics_family, present_family;
        bool complete() const {
            const std::vector<bool> families_found {
                this->graphics_family.has_value(),
                this->present_family.has_value(),
            };
            for (bool found : families_found) {
                if (!found) {
                    return false;
                }
            }
            return true;
        }
    };
    struct sync_objects {
        VkSemaphore image_available_semaphore = nullptr;
        VkSemaphore render_finished_semaphore = nullptr;
        VkFence fence = nullptr;
    };
#endif
    class renderer {
    public:
        static void add_layer(const std::string& name);
        static void add_instance_extension(const std::string& name);
        static void add_device_extension(const std::string& name);
        static void init(ref<window> _window);
        static void shutdown();
        static void new_frame();
        static void add_ref();
        static void remove_ref();
        static void render(ref<command_buffer> cmdbuffer, entity to_render);
        static ref<command_buffer> create_render_command_buffer();
        static ref<command_buffer> create_single_time_command_buffer();
        static ref<window> get_window();
        static VkInstance get_instance();
        static VkPhysicalDevice get_physical_device();
        static VkDevice get_device();
        static VkQueue get_graphics_queue();
        static VkQueue get_present_queue();
        static VkSurfaceKHR get_window_surface();
        static VkDescriptorPool get_descriptor_pool();
        static ref<texture> get_white_texture();
        static ref<uniform_buffer> get_camera_buffer();
        static void update_camera_buffer(ref<scene> _scene); // todo: add window parameter after window surface is moved to swapchain
        static void expand_vulkan_version(uint32_t version, uint32_t& major, uint32_t& minor, uint32_t& patch);
#ifdef EXPOSE_RENDERER_INTERNALS
        static swapchain_support_details query_swapchain_support(VkPhysicalDevice device);
        static queue_family_indices find_queue_families(VkPhysicalDevice device);
        static const sync_objects& get_sync_objects(size_t frame_index);
        static size_t get_current_frame();
        static constexpr size_t max_frame_count = 2;
#endif
    private:
        renderer() = default;
    };
}