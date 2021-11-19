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
        this->destroy();
        this->create(width, height);
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
    static VkSwapchainKHR create_swapchain(uint32_t width, uint32_t height) {
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
        create_info.imageFormat = format.format;
        create_info.imageColorSpace = format.colorSpace;
        create_info.imageExtent = choose_extent(width, height, support_details.capabilities);
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
        VkSwapchainKHR swapchain_object;
        if (vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain_object) != VK_SUCCESS) {
            throw std::runtime_error("could not create swapchain");
        }
        return swapchain_object;
    }
    void swapchain::create(int32_t width, int32_t height) {
        this->m_swapchain = create_swapchain((uint32_t)width, (uint32_t)height);
    }
    void swapchain::destroy() {
        VkDevice device = renderer::get_device();
        vkDestroySwapchainKHR(device, this->m_swapchain, nullptr);
    }
}