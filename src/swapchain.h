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
namespace vkrollercoaster {
    class pipeline;
    class swapchain {
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
        VkSwapchainKHR get_swapchain() { return this->m_swapchain; }
        const std::vector<swapchain_image>& get_swapchain_images() { return this->m_swapchain_images; }
        VkFormat get_image_format() { return this->m_image_format; }
        VkExtent2D get_extent() { return this->m_extent; }
        VkRenderPass get_render_pass() { return this->m_render_pass; }
    private:
        void create(int32_t width, int32_t height);
        void create_swapchain(uint32_t width, uint32_t height);
        void create_render_pass();
        void fetch_images();
        void destroy();
        std::shared_ptr<window> m_window;
        VkSwapchainKHR m_swapchain;
        VkFormat m_image_format;
        VkExtent2D m_extent;
        VkRenderPass m_render_pass;
        std::vector<swapchain_image> m_swapchain_images;
        std::unordered_set<pipeline*> m_dependents;
        friend class pipeline;
    };
}