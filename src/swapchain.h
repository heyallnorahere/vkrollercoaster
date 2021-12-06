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
#include "framebuffer.h"
namespace vkrollercoaster {
    class swapchain : public render_target {
    public:
        struct swapchain_image {
            VkImage image;
            VkImageView view;
            VkFramebuffer framebuffer;
        };
        swapchain();
        virtual ~swapchain() override;
        swapchain(const swapchain&) = delete;
        swapchain& operator=(const swapchain&) = delete;
        void reload();
        void prepare_frame();
        void present();
        virtual void add_reload_callbacks(void* id, std::function<void()> destroy, std::function<void()> recreate) override;
        virtual void remove_reload_callbacks(void* id) override;
        VkSwapchainKHR get_swapchain() { return this->m_swapchain; }
        const std::vector<swapchain_image>& get_swapchain_images() { return this->m_swapchain_images; }
        VkFormat get_image_format() { return this->m_image_format; }
        virtual VkExtent2D get_extent() override { return this->m_extent; }
        virtual VkRenderPass get_render_pass() override { return this->m_render_pass; }
        virtual VkFramebuffer get_framebuffer() override { return this->m_swapchain_images[this->m_current_image].framebuffer; }
        virtual void get_attachment_types(std::set<framebuffer_attachment_type>& types) override { types = { framebuffer_attachment_type::color, framebuffer_attachment_type::depth_stencil }; }
        virtual render_target_type get_render_target_type() override { return render_target_type::swapchain; }
        uint32_t get_current_image() { return this->m_current_image; }
        ref<window> get_window() { return this->m_window; }
        ref<image> get_depth_image() { return this->m_depth_image; }
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