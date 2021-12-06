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
#include "menus.h"
#include "../application.h"
namespace vkrollercoaster {
    static viewport* viewport_instance = nullptr;
    ref<viewport> viewport::get_instance() {
        return viewport_instance;
    }

    viewport::viewport() {
        if (viewport_instance) {
            throw std::runtime_error("cannot have more than 1 viewport window!");
        }
        viewport_instance = this;

        framebuffer_spec spec;

        // let's initialize this to the size of the swapchain
        ref<swapchain> swap_chain = application::get_swapchain();
        VkExtent2D swapchain_extent = swap_chain->get_extent();
        spec.width = swapchain_extent.width;
        spec.height = swapchain_extent.height;

        // we want both a color and depth attachment
        spec.requested_attachments[framebuffer_attachment_type::color] = VK_FORMAT_R8G8B8A8_UNORM;
        spec.requested_attachments[framebuffer_attachment_type::depth_stencil] = VK_FORMAT_D32_SFLOAT;

        this->m_framebuffer = ref<framebuffer>::create(spec);

        // create texture for displaying
        this->m_color_attachment = ref<texture>::create(this->m_framebuffer->get_attachment(framebuffer_attachment_type::color));
    }
    
    viewport::~viewport() {
        viewport_instance = nullptr;
    }

    void viewport::update() {
        ImGui::Begin("Viewport", &this->m_open);

        ImVec2 available_content_region = ImGui::GetContentRegionAvail();
        ImGui::Image(this->m_color_attachment->get_imgui_id(), available_content_region);

        ImGui::End();

        this->verify_size(available_content_region);
    }

    void viewport::verify_size(ImVec2 available_content_region) {
        VkExtent2D framebuffer_extent = this->m_framebuffer->get_extent();
        VkExtent2D window_size;
        window_size.width = (uint32_t)available_content_region.x;
        if (window_size.width == 0) {
            window_size.width = 100;
        }
        window_size.height = (uint32_t)available_content_region.y;
        if (window_size.height == 0) {
            window_size.height = 100;
        }

        if (framebuffer_extent.width != window_size.width || framebuffer_extent.height != window_size.height) {
            // resize the framebuffer
            this->m_framebuffer->resize(window_size);
            
            // save the previous color attachment so it doesn't get deleted
            this->m_previous_color_attachment = this->m_color_attachment;

            // recreate the texture
            this->m_color_attachment = ref<texture>::create(this->m_framebuffer->get_attachment(framebuffer_attachment_type::color));
        }
    }
}