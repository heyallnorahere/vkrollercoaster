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
#include "image.h"
namespace vkrollercoaster {
    class swapchain : public ref_counted {
    public:
        struct swapchain_image {
            VkImage image;
            VkImageView view;
            VkFramebuffer framebuffer;
        };
        swapchain();
        ~swapchain();
        swapchain(const swapchain&) = delete;
        swapchain& operator=(const swapchain&) = delete;
        void reload();
        void prepare_frame();
        void present();
        void add_reload_callbacks(void* id, std::function<void()> destroy, std::function<void()> recreate);
        void remove_reload_callbacks(void* id);
        VkSwapchainKHR get_swapchain() { return this->m_swapchain; }
        const std::vector<swapchain_image>& get_swapchain_images() { return this->m_swapchain_images; }
        VkFormat get_image_format() { return this->m_image_format; }
        VkExtent2D get_extent() { return this->m_extent; }
        VkRenderPass get_render_pass() { return this->m_render_pass; }
        uint32_t get_current_image() { return this->m_current_image; }
        ref<window> get_window() { return this->m_window; }
    private:
        struct swapchain_dependent {
            std::function<void()> destroy, recreate;
        };
        void create(int32_t width, int32_t height, bool render_pass = false);
        void create_swapchain(uint32_t width, uint32_t height);
        void create_depth_image();
        void create_render_pass();
        void fetch_images();
        void destroy();
        ref<window> m_window;
        VkSwapchainKHR m_swapchain;
        VkFormat m_image_format;
        VkExtent2D m_extent;
        VkRenderPass m_render_pass;
        ref<image> m_depth_image;
        std::vector<swapchain_image> m_swapchain_images;
        std::map<void*, swapchain_dependent> m_dependents;
        uint32_t m_current_image;
        std::vector<VkFence> m_image_fences;
        bool m_should_resize;
        friend class window;
    };
}