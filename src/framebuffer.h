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
#include "image.h"
#include "render_target.h"
namespace vkrollercoaster {
    struct framebuffer_spec {
        uint32_t width = 0, height = 0;
        VkRenderPass render_pass = nullptr;
        VkFramebuffer framebuffer =
            nullptr; // passing a framebuffer gives ownership over to the framebuffer class
        std::map<attachment_type, VkFormat> requested_attachments;
        std::map<attachment_type, ref<image>> provided_attachments;
    };
    class framebuffer : public render_target {
    public:
        framebuffer(const framebuffer_spec& spec);
        virtual ~framebuffer() override;
        framebuffer(const framebuffer&) = delete;
        framebuffer& operator=(const framebuffer&) = delete;
        virtual render_target_type get_render_target_type() override {
            return render_target_type::framebuffer;
        }
        virtual VkRenderPass get_render_pass() override { return this->m_render_pass; }
        virtual VkFramebuffer get_framebuffer() override { return this->m_framebuffer; }
        virtual VkExtent2D get_extent() override { return this->m_extent; }
        virtual void add_reload_callbacks(void* id, std::function<void()> destroy,
                                          std::function<void()> recreate) override;
        virtual void remove_reload_callbacks(void* id) override;
        virtual void get_attachment_types(std::set<attachment_type>& types) override;
        ref<image> get_attachment(attachment_type type);
        void set_attachment(attachment_type type, ref<image> attachment);
        void reload();
        void resize(VkExtent2D new_size);

    private:
        struct framebuffer_dependent {
            std::function<void()> destroy, recreate;
        };
        void acquire_attachments(const framebuffer_spec& spec);
        void create_render_pass();
        void create_framebuffer();
        void destroy_framebuffer(bool invoke_callbacks = true);
        VkExtent2D m_extent;
        VkRenderPass m_render_pass;
        bool m_render_pass_owned;
        VkFramebuffer m_framebuffer;
        std::map<attachment_type, ref<image>> m_attachments;
        std::map<void*, framebuffer_dependent> m_dependents;
    };
} // namespace vkrollercoaster