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
#include "framebuffer.h"
#include "renderer.h"
#include "util.h"
namespace vkrollercoaster {
    framebuffer::framebuffer(const framebuffer_spec& spec) {
        if (spec.width == 0) {
            throw std::runtime_error("the specified width must be more than zero!");
        }
        if (spec.height == 0) {
            throw std::runtime_error("the specified height must be more than zero!");
        }
        this->m_extent.width = spec.width;
        this->m_extent.height = spec.height;

        this->acquire_attachments(spec);
        renderer::add_ref();
        if (spec.render_pass) {
            this->m_render_pass = spec.render_pass;
            this->m_render_pass_owned = false;
        } else {
            this->create_render_pass();
            this->m_render_pass_owned = true;
        }
        if (spec.framebuffer) {
            this->m_framebuffer = spec.framebuffer;
        } else {
            this->create_framebuffer();
        }
    }

    framebuffer::~framebuffer() {
        this->destroy_framebuffer(false);
        if (this->m_render_pass_owned) {
            VkDevice device = renderer::get_device();
            vkDestroyRenderPass(device, this->m_render_pass, nullptr);
        }
        renderer::remove_ref();
    }

    void framebuffer::add_reload_callbacks(void* id, std::function<void()> destroy,
                                           std::function<void()> recreate) {
        if (this->m_dependents.find(id) != this->m_dependents.end()) {
            return;
        }
        framebuffer_dependent callbacks;
        callbacks.destroy = destroy;
        callbacks.recreate = recreate;
        this->m_dependents.insert(std::make_pair(id, callbacks));
    }

    void framebuffer::remove_reload_callbacks(void* id) { this->m_dependents.erase(id); }

    void framebuffer::get_attachment_types(std::set<attachment_type>& types) {
        types.clear();
        for (const auto& [type, attachment] : this->m_attachments) {
            types.insert(type);
        }
    }

    ref<image> framebuffer::get_attachment(attachment_type type) {
        ref<image> attachment;
        if (this->m_attachments.find(type) != this->m_attachments.end()) {
            attachment = this->m_attachments[type];
        }
        return attachment;
    }

    void framebuffer::set_attachment(attachment_type type, ref<image> attachment) {
        bool recreate = false;
        if (this->m_framebuffer) {
            this->destroy_framebuffer();
            recreate = true;
        }
        this->m_attachments[type] = attachment;
        if (recreate) {
            this->create_framebuffer();
        }
    }

    void framebuffer::reload() {
        this->destroy_framebuffer();
        this->create_framebuffer();
    }

    void framebuffer::resize(VkExtent2D new_size) {
        this->destroy_framebuffer();
        this->m_extent = new_size;
        framebuffer_spec spec;
        spec.width = new_size.width;
        spec.height = new_size.height;
        for (auto [type, attachment] : this->m_attachments) {
            spec.requested_attachments[type] = attachment->get_format();
        }
        this->acquire_attachments(spec);
        this->create_framebuffer();
    }

    void framebuffer::acquire_attachments(const framebuffer_spec& spec) {
        // copy the provided attachment images
        this->m_attachments = spec.provided_attachments;

        // create images for the requested attachments
        for (const auto& [type, format] : spec.requested_attachments) {
            if (this->m_attachments.find(type) != this->m_attachments.end()) {
                continue;
            }

            VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            VkImageAspectFlags image_aspect = 0;
            switch (type) {
            case attachment_type::color:
                usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
                image_aspect |= VK_IMAGE_ASPECT_COLOR_BIT;
                break;
            case attachment_type::depth_stencil:
                usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                image_aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
                if (format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                    format == VK_FORMAT_D24_UNORM_S8_UINT) {
                    image_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
                }
                break;
            }

            ref<image> attachment =
                ref<image2d>::create(format, spec.width, spec.height, usage, image_aspect);
            this->m_attachments[type] = attachment;
        }
    }

    void framebuffer::create_render_pass() {
        std::map<attachment_type, size_t> attachment_ref_indices;
        std::vector<VkAttachmentDescription> attachments;
        std::vector<VkAttachmentReference> attachment_refs;
        for (auto [type, attachment] : this->m_attachments) {
            VkAttachmentReference attachment_ref;
            util::zero(attachment_ref);
            attachment_ref.attachment = attachments.size();
            attachment_ref.layout = attachment->get_layout();
            attachment_ref_indices[type] = attachment_refs.size();
            attachment_refs.push_back(attachment_ref);
            VkAttachmentDescription attachment_desc;
            util::zero(attachment_desc);
            attachment_desc.format = attachment->get_format();
            attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment_desc.finalLayout = attachment->get_layout();
            attachments.push_back(attachment_desc);
        }
        VkSubpassDescription subpass;
        util::zero(subpass);
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        if (attachment_ref_indices.find(attachment_type::color) != attachment_ref_indices.end()) {
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments =
                &attachment_refs[attachment_ref_indices[attachment_type::color]];
        }
        if (attachment_ref_indices.find(attachment_type::depth_stencil) !=
            attachment_ref_indices.end()) {
            subpass.pDepthStencilAttachment =
                &attachment_refs[attachment_ref_indices[attachment_type::depth_stencil]];
        }
        VkRenderPassCreateInfo create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attachments.size();
        create_info.pAttachments = attachments.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        VkDevice device = renderer::get_device();
        if (vkCreateRenderPass(device, &create_info, nullptr, &this->m_render_pass) != VK_SUCCESS) {
            throw std::runtime_error("could not create render pass!");
        }
    }

    void framebuffer::create_framebuffer() {
        std::vector<VkImageView> attachments;
        for (auto [type, attachment] : this->m_attachments) {
            attachments.push_back(attachment->get_view());
        }
        VkFramebufferCreateInfo create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        create_info.renderPass = this->m_render_pass;
        create_info.attachmentCount = attachments.size();
        create_info.pAttachments = attachments.data();
        create_info.width = this->m_extent.width;
        create_info.height = this->m_extent.height;
        create_info.layers = 1;
        VkDevice device = renderer::get_device();
        if (vkCreateFramebuffer(device, &create_info, nullptr, &this->m_framebuffer) !=
            VK_SUCCESS) {
            throw std::runtime_error("could not create framebuffer!");
        }
        for (const auto& [id, callbacks] : this->m_dependents) {
            callbacks.recreate();
        }
    }

    void framebuffer::destroy_framebuffer(bool invoke_callbacks) {
        if (invoke_callbacks) {
            for (const auto& [id, callbacks] : this->m_dependents) {
                callbacks.destroy();
            }
        }
        VkDevice device = renderer::get_device();
        vkDestroyFramebuffer(device, this->m_framebuffer, nullptr);
    }
} // namespace vkrollercoaster