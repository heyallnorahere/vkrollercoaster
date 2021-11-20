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
#include "swapchain.h"
#define EXPOSE_RENDERER_INTERNALS
#include "renderer.h"
#include "util.h"
#include "pipeline.h"
namespace vkrollercoaster {
    swapchain::swapchain() {
        renderer::add_ref();
        this->m_window = renderer::get_window();
        int32_t width, height;
        this->m_window->get_size(&width, &height);
        this->create(width, height);
    }
    swapchain::~swapchain() {
        this->destroy();
        renderer::remove_ref();
    }
    void swapchain::reload() {
        int32_t width = 0, height = 0;
        this->m_window->get_size(&width, &height);
        if (width == 0 || height == 0) {
            this->m_window->get_size(&width, &height);
            glfwWaitEvents();
        }
        for (auto _pipeline : this->m_dependents) {
            _pipeline->destroy_pipeline();
        }
        this->destroy();
        this->create(width, height);
        for (auto _pipeline : this->m_dependents) {
            _pipeline->create_pipeline();
        }
    }
    static VkSurfaceFormatKHR choose_format(const std::vector<VkSurfaceFormatKHR>& formats) {
        constexpr VkFormat preferred_format = VK_FORMAT_R8G8B8A8_SRGB;
        constexpr VkColorSpaceKHR preferred_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        for (const auto& format : formats) {
            if (format.format == preferred_format && format.colorSpace == preferred_color_space) {
                return format;
            }
        }
        return formats[0];
    }
    static VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& present_modes) {
        for (auto present_mode : present_modes) {
            if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return present_mode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }
    static VkExtent2D choose_extent(uint32_t width, uint32_t height, const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            VkExtent2D extent;
            extent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            extent.height = std::clamp(height, capabilities.minImageExtent.width, capabilities.maxImageExtent.height);
            return extent;
        }
    }
    void swapchain::create(int32_t width, int32_t height) {
        this->create_swapchain((uint32_t)width, (uint32_t)height);
        this->create_render_pass();
        this->fetch_images();
    }
    void swapchain::create_swapchain(uint32_t width, uint32_t height) {
        auto physical_device = renderer::get_physical_device();
        auto support_details = query_swapchain_support(physical_device);
        auto indices = find_queue_families(physical_device);
        uint32_t image_count = support_details.capabilities.minImageCount + 1;
        if (support_details.capabilities.maxImageCount > 0 && image_count > support_details.capabilities.maxImageCount) {
            image_count = support_details.capabilities.maxImageCount;
        }
        VkSwapchainCreateInfoKHR create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = renderer::get_window_surface();
        create_info.minImageCount = image_count;
        auto format = choose_format(support_details.formats);
        this->m_image_format = create_info.imageFormat = format.format;
        create_info.imageColorSpace = format.colorSpace;
        this->m_extent = create_info.imageExtent = choose_extent(width, height, support_details.capabilities);
        create_info.imageArrayLayers = 1;
        create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        std::vector<uint32_t> queue_family_index_array = { *indices.graphics_family, *indices.present_family };
        if (indices.graphics_family != indices.present_family) {
            create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = queue_family_index_array.size();
            create_info.pQueueFamilyIndices = queue_family_index_array.data();
        } else {
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        create_info.preTransform = support_details.capabilities.currentTransform;
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info.presentMode = choose_present_mode(support_details.present_modes);
        create_info.clipped = true;
        VkDevice device = renderer::get_device();
        if (vkCreateSwapchainKHR(device, &create_info, nullptr, &this->m_swapchain) != VK_SUCCESS) {
            throw std::runtime_error("could not create swapchain");
        }
    }
    void swapchain::create_render_pass() {
        VkAttachmentDescription color_attachment;
        util::zero(color_attachment);
        color_attachment.format = this->m_image_format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        // for use later with depth buffering
        std::vector<VkAttachmentDescription> attachments = { color_attachment };
        VkAttachmentReference color_attachment_ref;
        util::zero(color_attachment_ref);
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass;
        util::zero(subpass);
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;
        VkRenderPassCreateInfo create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attachments.size();
        create_info.pAttachments = attachments.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        VkDevice device = renderer::get_device();
        if (vkCreateRenderPass(device, &create_info, nullptr, &this->m_render_pass) != VK_SUCCESS) {
            throw std::runtime_error("could not create render pass for swapchain!");
        }
    }
    void swapchain::fetch_images() {
        VkDevice device = renderer::get_device();
        uint32_t image_count;
        vkGetSwapchainImagesKHR(device, this->m_swapchain, &image_count, nullptr);
        std::vector<VkImage> images(image_count);
        vkGetSwapchainImagesKHR(device, this->m_swapchain, &image_count, images.data());
        for (VkImage image : images) {
            auto& image_desc = this->m_swapchain_images.emplace_back();
            image_desc.image = image;
            VkImageViewCreateInfo view_create_info;
            util::zero(view_create_info);
            view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_create_info.image = image;
            view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_create_info.format = this->m_image_format;
            view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_create_info.subresourceRange.baseMipLevel = 0;
            view_create_info.subresourceRange.levelCount = 1;
            view_create_info.subresourceRange.baseArrayLayer = 0;
            view_create_info.subresourceRange.layerCount = 1;
            if (vkCreateImageView(device, &view_create_info, nullptr, &image_desc.view) != VK_SUCCESS) {
                throw std::runtime_error("could not create swapchain image view!");
            }
            std::vector<VkImageView> attachments = { image_desc.view };
            VkFramebufferCreateInfo fb_create_info;
            util::zero(fb_create_info);
            fb_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fb_create_info.renderPass = this->m_render_pass;
            fb_create_info.attachmentCount = attachments.size();
            fb_create_info.pAttachments = attachments.data();
            fb_create_info.width = this->m_extent.width;
            fb_create_info.height = this->m_extent.height;
            fb_create_info.layers = 1;
            if (vkCreateFramebuffer(device, &fb_create_info, nullptr, &image_desc.framebuffer) != VK_SUCCESS) {
                throw std::runtime_error("could not create swapchain framebuffer!");
            }
        }
    }
    void swapchain::destroy() {
        VkDevice device = renderer::get_device();
        for (const auto& image : this->m_swapchain_images) {
            vkDestroyFramebuffer(device, image.framebuffer, nullptr);
            vkDestroyImageView(device, image.view, nullptr);
        }
        this->m_swapchain_images.clear();
        vkDestroyRenderPass(device, this->m_render_pass, nullptr);
        vkDestroySwapchainKHR(device, this->m_swapchain, nullptr);
    }
}