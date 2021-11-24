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
#define EXPOSE_RENDERER_INTERNALS
#include "image.h"
#include "buffers.h"
#include "renderer.h"
#include "util.h"
#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
namespace vkrollercoaster {
    void create_image(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& memory) {
        VkDevice device = renderer::get_device();
        VkPhysicalDevice physical_device = renderer::get_physical_device();
        VkImageCreateInfo create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        create_info.imageType = VK_IMAGE_TYPE_2D; // ill change this if/when i allow settings to be passed
        create_info.extent.width = (uint32_t)width;
        create_info.extent.height = (uint32_t)height;
        create_info.extent.depth = 1;
        create_info.mipLevels = 1;
        create_info.arrayLayers = 1;
        create_info.format = format;
        create_info.tiling = tiling;
        create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        create_info.usage = usage;
        create_info.samples = VK_SAMPLE_COUNT_1_BIT;
        // todo: if graphics_family and compute_family arent similar, use VK_SHARING_MODE_CONCURRENT
        //auto indices = renderer::find_queue_families(physical_device);
        create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(device, &create_info, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("could not create image!");
        }
        VkMemoryRequirements requirements;
        vkGetImageMemoryRequirements(device, image, &requirements);
        VkMemoryAllocateInfo alloc_info;
        util::zero(alloc_info);
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = requirements.size;
        alloc_info.memoryTypeIndex = find_memory_type(requirements.memoryTypeBits, properties);
        // todo: use vma
        if (vkAllocateMemory(device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
            throw std::runtime_error("could not allocate memory for image!");
        }
        vkBindImageMemory(device, image, memory, 0);
    }
    static void get_stage_and_mask(VkImageLayout layout, VkPipelineStageFlags& stage, VkAccessFlags& access_mask) {
        switch (layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            access_mask = 0;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            access_mask = VK_ACCESS_SHADER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_GENERAL:
            stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            break;
        default:
            throw std::runtime_error("unimplemented/unsupported image layout!");
        }
    }
    void transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout) {
        VkImageMemoryBarrier barrier;
        util::zero(barrier);
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        // todo: take image aspect flags
        if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        } else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        VkPipelineStageFlags source_stage, destination_stage;
        get_stage_and_mask(old_layout, source_stage, barrier.srcAccessMask);
        get_stage_and_mask(new_layout, destination_stage, barrier.dstAccessMask);
        auto cmdbuffer = renderer::create_single_time_command_buffer();
        cmdbuffer->begin();
        vkCmdPipelineBarrier(cmdbuffer->get(),
            source_stage, destination_stage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        cmdbuffer->end();
        cmdbuffer->submit();
    }
    static void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkBufferImageCopy region;
        util::zero(region);
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { width, height, 1 };
        auto cmdbuffer = renderer::create_single_time_command_buffer();
        cmdbuffer->begin();
        vkCmdCopyBufferToImage(cmdbuffer->get(), buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        cmdbuffer->end();
        cmdbuffer->submit();
    }
    bool image::load_image(const fs::path& path, image_data& data) {
        std::string string_path = path.string();
        uint8_t* raw_data = stbi_load(string_path.c_str(), &data.width, &data.height, &data.channels, 0);
        if (!raw_data) {
            return false;
        }
        size_t byte_count = (size_t)data.width * data.height * data.channels;
        data.data.resize(byte_count);
        // probably shouldnt use memcpy
        memcpy(data.data.data(), raw_data, byte_count * sizeof(uint8_t));
        stbi_image_free(raw_data);
        return true;
    }
    std::shared_ptr<image> image::from_file(const fs::path& path) {
        std::shared_ptr<image> created_image;
        image_data data;
        if (load_image(path, data)) {
            created_image = std::make_shared<image>(data);
        }
        return created_image;
    }
    image::image(const image_data& data) {
        renderer::add_ref();
        this->m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        this->m_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        this->create_image_from_data(data);
        this->create_view();
    }
    image::image(VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usage, VkImageAspectFlags aspect) {
        // only use this constructor for things such as depth buffering
        renderer::add_ref();
        this->m_format = format;
        this->m_aspect = aspect;
        this->m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        create_image(width, height, this->m_format, VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            this->m_image, this->m_memory);
        this->create_view();
    }
    image::~image() {
        VkDevice device = renderer::get_device();
        vkDestroyImageView(device, this->m_view, nullptr);
        vkDestroyImage(device, this->m_image, nullptr);
        vkFreeMemory(device, this->m_memory, nullptr);
        renderer::remove_ref();
    }
    void image::transition(VkImageLayout new_layout) {
        transition_image_layout(this->m_image, this->m_format, this->m_layout, new_layout);
        this->m_layout = new_layout;
    }
    void image::create_image_from_data(const image_data& data) {
        switch (data.channels) {
        case 4:
            this->m_format = VK_FORMAT_R8G8B8A8_SRGB;
            break;
        case 3:
            this->m_format = VK_FORMAT_R8G8B8_SRGB;
            break;
        default:
            throw std::runtime_error("invalid image format!");
        }
        VkBuffer staging_buffer;
        VkDeviceMemory staging_memory;
        size_t total_size = (size_t)data.width * data.height * data.channels;
        create_buffer(total_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer, staging_memory);
        VkDevice device = renderer::get_device();
        void* gpu_data;
        vkMapMemory(device, staging_memory, 0, total_size, 0, &gpu_data);
        memcpy(gpu_data, data.data.data(), total_size);
        vkUnmapMemory(device, staging_memory);
        create_image(data.width, data.height, this->m_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, this->m_image, this->m_memory);
        this->transition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copy_buffer_to_image(staging_buffer, this->m_image, data.width, data.height);
        this->transition(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_memory, nullptr);
    }
    void image::create_view() {
        VkImageViewCreateInfo create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = this->m_image;
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = this->m_format;
        create_info.subresourceRange.aspectMask = this->m_aspect;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;
        VkDevice device = renderer::get_device();
        if (vkCreateImageView(device, &create_info, nullptr, &this->m_view) != VK_SUCCESS) {
            throw std::runtime_error("could not create image view!");
        }
    }
}