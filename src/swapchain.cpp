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
namespace vkrollercoaster {
    /*
                    VkBool32 present_support = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, renderer_data.window_surface,
                                                     &present_support);
                if (present_support) {
                    indices.present_family = i;
                }
    */
    struct swapchain_support_details {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
    };
    static swapchain_support_details query_swapchain_support(VkSurfaceKHR surface) {
        VkPhysicalDevice device = renderer::get_physical_device();

        swapchain_support_details details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
        if (format_count > 0) {
            details.formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count,
                                                 details.formats.data());
        }
        uint32_t present_mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);
        if (format_count > 0) {
            details.present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count,
                                                      details.present_modes.data());
        }
        return details;
    }

    swapchain::swapchain(ref<window> _window) {
        renderer::add_ref();
        this->m_should_resize = false;
        this->m_current_image = 0;
        this->m_window = _window;
        int32_t width, height;
        this->m_window->get_size(&width, &height);
        this->create(width, height, true);
        this->m_window->m_swapchains.insert(this);
    }

    swapchain::~swapchain() {
        this->m_window->m_swapchains.erase(this);
        this->destroy();

        VkDevice device = renderer::get_device();
        vkDestroyRenderPass(device, this->m_render_pass, nullptr);

        VkInstance instance = renderer::get_instance();
        vkDestroySurfaceKHR(instance, this->m_surface, nullptr);

        renderer::remove_ref();
    }

    void swapchain::reload() {
        int32_t width = 0, height = 0;
        this->m_window->get_size(&width, &height);
        if (width == 0 || height == 0) {
            this->m_window->get_size(&width, &height);
            glfwWaitEvents();
        }
        for (const auto& [_, callbacks] : this->m_dependents) {
            callbacks.destroy();
        }
        this->destroy();
        this->create(width, height);
        for (const auto& [_, callbacks] : this->m_dependents) {
            callbacks.recreate();
        }
    }
    void swapchain::prepare_frame() {
        VkDevice device = renderer::get_device();
        size_t current_frame = renderer::get_current_frame();
        const auto& frame_sync_objects = renderer::get_sync_objects(current_frame);
        constexpr uint64_t uint64_max = std::numeric_limits<uint64_t>::max();
        vkWaitForFences(device, 1, &frame_sync_objects.fence, true, uint64_max);

        VkResult result = vkAcquireNextImageKHR(device, this->m_swapchain, uint64_max,
                                                frame_sync_objects.image_available_semaphore,
                                                nullptr, &this->m_current_image);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            this->reload();
            this->prepare_frame();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("could not acquire next swapchain image!");
        }

        if (this->m_image_fences[this->m_current_image] != nullptr) {
            vkWaitForFences(device, 1, &this->m_image_fences[this->m_current_image], true,
                            uint64_max);
        }
        this->m_image_fences[this->m_current_image] = frame_sync_objects.fence;
        vkResetFences(device, 1, &frame_sync_objects.fence);
    }
    void swapchain::present() {
        VkDevice device = renderer::get_device();
        VkQueue present_queue;
        vkGetDeviceQueue(device, this->m_present_family, 0, &present_queue);

        size_t current_frame = renderer::get_current_frame();
        const auto& frame_sync_objects = renderer::get_sync_objects(current_frame);

        VkPresentInfoKHR present_info;
        util::zero(present_info);
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &frame_sync_objects.render_finished_semaphore;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &this->m_swapchain;
        present_info.pImageIndices = &this->m_current_image;

        VkResult result = vkQueuePresentKHR(present_queue, &present_info);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
            this->m_should_resize) {
            this->m_should_resize = false;
            this->reload();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("could not present!");
        }
    }
    void swapchain::add_reload_callbacks(void* id, std::function<void()> destroy,
                                         std::function<void()> recreate) {
        if (this->m_dependents.find(id) != this->m_dependents.end()) {
            throw std::runtime_error("the given id already exists!");
        }
        swapchain_dependent dependent_desc;
        dependent_desc.destroy = destroy;
        dependent_desc.recreate = recreate;
        this->m_dependents.insert({ id, dependent_desc });
    }
    void swapchain::remove_reload_callbacks(void* id) { this->m_dependents.erase(id); }
    static VkSurfaceFormatKHR choose_format(const std::vector<VkSurfaceFormatKHR>& formats) {
        std::vector<VkFormat> preferred_formats = { VK_FORMAT_B8G8R8A8_UNORM,
                                                    VK_FORMAT_R8G8B8A8_UNORM,
                                                    VK_FORMAT_B8G8R8_UNORM,
                                                    VK_FORMAT_R8G8B8_UNORM };
        constexpr VkColorSpaceKHR preferred_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        if (formats.size() == 1) {
            if (formats[0].format == VK_FORMAT_UNDEFINED) {
                VkSurfaceFormatKHR surface_format;
                surface_format.format = preferred_formats[0];
                surface_format.colorSpace = preferred_color_space;
            }
        } else {
            for (VkFormat preferred_format : preferred_formats) {
                for (const auto& format : formats) {
                    if (format.format == preferred_format &&
                        format.colorSpace == preferred_color_space) {
                        return format;
                    }
                }
            }
        }
        return formats[0];
    }
    static VkPresentModeKHR choose_present_mode(
        const std::vector<VkPresentModeKHR>& present_modes) {
        std::vector<VkPresentModeKHR> preferred_present_modes = { VK_PRESENT_MODE_MAILBOX_KHR,
                                                                  VK_PRESENT_MODE_IMMEDIATE_KHR,
                                                                  VK_PRESENT_MODE_FIFO_KHR };
        for (auto preferred_present_mode : preferred_present_modes) {
            for (auto present_mode : present_modes) {
                if (present_mode == preferred_present_mode) {
                    return present_mode;
                }
            }
        }
        return preferred_present_modes[0];
    }
    static VkExtent2D choose_extent(uint32_t width, uint32_t height,
                                    const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            VkExtent2D extent;
            extent.width = std::clamp(width, capabilities.minImageExtent.width,
                                      capabilities.maxImageExtent.width);
            extent.height = std::clamp(height, capabilities.minImageExtent.width,
                                       capabilities.maxImageExtent.height);
            return extent;
        }
    }
    void swapchain::create(int32_t width, int32_t height, bool init) {
        if (init) {
            this->create_surface();
        }

        this->create_swapchain((uint32_t)width, (uint32_t)height);
        this->create_depth_image();

        if (init) {
            this->create_render_pass();
        }

        this->fetch_images();
    }
    void swapchain::create_surface() {
        GLFWwindow* glfw_window = this->m_window->get();
        VkInstance instance = renderer::get_instance();
        if (glfwCreateWindowSurface(instance, glfw_window, nullptr, &this->m_surface) != VK_SUCCESS) {
            throw std::runtime_error("could not create window surface!");
        }

        VkPhysicalDevice physical_device = renderer::get_physical_device();
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

        std::optional<uint32_t> present_family;
        for (uint32_t i = 0; i < queue_family_count; i++) {
            VkBool32 presentation_supported = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, this->m_surface, &presentation_supported);
            if (presentation_supported) {
                present_family = i;
            }
        }

        if (!present_family) {
            throw std::runtime_error("could not find present family!");
        }

        std::set<uint32_t> created_queues = renderer::find_queue_families(physical_device).create_set();
        if (created_queues.find(*present_family) == created_queues.end()) {
            throw std::runtime_error("the present queue was not created!");
        }

        this->m_present_family = *present_family;
    }
    void swapchain::create_swapchain(uint32_t width, uint32_t height) {
        auto physical_device = renderer::get_physical_device();
        auto support_details = query_swapchain_support(this->m_surface);
        auto indices = renderer::find_queue_families(physical_device);
        uint32_t image_count = support_details.capabilities.minImageCount + 1;
        if (support_details.capabilities.maxImageCount > 0 &&
            image_count > support_details.capabilities.maxImageCount) {
            image_count = support_details.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = this->m_surface;
        create_info.minImageCount = image_count;
        auto format = choose_format(support_details.formats);
        this->m_image_format = create_info.imageFormat = format.format;
        create_info.imageColorSpace = format.colorSpace;
        this->m_extent = create_info.imageExtent =
            choose_extent(width, height, support_details.capabilities);
        create_info.imageArrayLayers = 1;
        create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        std::vector<uint32_t> queue_family_index_array = { *indices.graphics_family,
                                                           this->m_present_family };
        if (indices.graphics_family != this->m_present_family) {
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
    static VkFormat find_supported_format(const std::vector<VkFormat>& candidates,
                                          VkImageTiling tiling, VkFormatFeatureFlags features) {
        VkPhysicalDevice physical_device = renderer::get_physical_device();
        for (VkFormat format : candidates) {
            VkFormatProperties properties;
            vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);
            if (tiling == VK_IMAGE_TILING_LINEAR &&
                (properties.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                       (properties.optimalTilingFeatures & features) == features) {
                return format;
            }
        }
        throw std::runtime_error("could not find a supported format!");
        return VK_FORMAT_MAX_ENUM;
    }
    void swapchain::create_depth_image() {
        VkFormat depth_format = find_supported_format(
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

        VkImageAspectFlags image_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            depth_format == VK_FORMAT_D24_UNORM_S8_UINT) {
            image_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        this->m_depth_image =
            ref<image>::create(depth_format, this->m_extent.width, this->m_extent.height,
                               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, image_aspect);
        this->m_depth_image->transition(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
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
        VkAttachmentDescription depth_attachment;
        util::zero(depth_attachment);
        depth_attachment.format = this->m_depth_image->get_format();
        depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment.finalLayout = this->m_depth_image->get_layout();
        std::vector<VkAttachmentDescription> attachments = { color_attachment, depth_attachment };
        VkAttachmentReference color_attachment_ref;
        util::zero(color_attachment_ref);
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference depth_attachment_ref;
        util::zero(depth_attachment_ref);
        depth_attachment_ref.attachment = 1;
        depth_attachment_ref.layout = this->m_depth_image->get_layout();
        VkSubpassDescription subpass;
        util::zero(subpass);
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;
        subpass.pDepthStencilAttachment = &depth_attachment_ref;
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
            if (vkCreateImageView(device, &view_create_info, nullptr, &image_desc.view) !=
                VK_SUCCESS) {
                throw std::runtime_error("could not create swapchain image view!");
            }
            std::vector<VkImageView> attachments = { image_desc.view,
                                                     this->m_depth_image->get_view() };
            VkFramebufferCreateInfo fb_create_info;
            util::zero(fb_create_info);
            fb_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fb_create_info.renderPass = this->m_render_pass;
            fb_create_info.attachmentCount = attachments.size();
            fb_create_info.pAttachments = attachments.data();
            fb_create_info.width = this->m_extent.width;
            fb_create_info.height = this->m_extent.height;
            fb_create_info.layers = 1;
            if (vkCreateFramebuffer(device, &fb_create_info, nullptr, &image_desc.framebuffer) !=
                VK_SUCCESS) {
                throw std::runtime_error("could not create swapchain framebuffer!");
            }
        }
        this->m_image_fences.resize(image_count, nullptr);
    }
    void swapchain::destroy() {
        VkDevice device = renderer::get_device();
        this->m_image_fences.clear();
        for (const auto& image : this->m_swapchain_images) {
            vkDestroyFramebuffer(device, image.framebuffer, nullptr);
            vkDestroyImageView(device, image.view, nullptr);
        }
        this->m_swapchain_images.clear();
        vkDestroySwapchainKHR(device, this->m_swapchain, nullptr);
    }
} // namespace vkrollercoaster