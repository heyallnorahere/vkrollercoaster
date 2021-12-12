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
namespace vkrollercoaster {
    enum class render_target_type { swapchain, framebuffer };
    enum class attachment_type { color, depth_stencil };

    class render_target : public ref_counted {
    public:
        virtual ~render_target() = default;

        virtual VkRenderPass get_render_pass() = 0;
        virtual VkFramebuffer get_framebuffer() = 0;
        virtual VkExtent2D get_extent() = 0;

        virtual void get_attachment_types(std::set<attachment_type>& types) = 0;
        virtual void add_reload_callbacks(void* id, std::function<void()> destroy,
                                          std::function<void()> recreate) = 0;
        virtual void remove_reload_callbacks(void* id) = 0;

        virtual render_target_type get_render_target_type() = 0;
    };
}