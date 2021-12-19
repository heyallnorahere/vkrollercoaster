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
#define EXPOSE_IMAGE_UTILS
#define EXPOSE_BUFFER_UTILS
#include "image.h"
#include "buffers.h"
#include "renderer.h"
#include "util.h"
#include "texture.h"
#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <ktx.h>
namespace vkrollercoaster {
    void create_image(const allocator& _allocator, uint32_t width, uint32_t height, uint32_t depth,
                      VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                      VmaMemoryUsage memory_usage, VkImage& image, VmaAllocation& allocation) {
        VkDevice device = renderer::get_device();
        VkPhysicalDevice physical_device = renderer::get_physical_device();
        VkImageCreateInfo create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        create_info.imageType =
            VK_IMAGE_TYPE_2D; // ill change this if/when i allow settings to be passed
        create_info.extent.width = width;
        create_info.extent.height = height;
        create_info.extent.depth = depth;
        create_info.mipLevels = 1;
        create_info.arrayLayers = 1;
        create_info.format = format;
        create_info.tiling = tiling;
        create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        create_info.usage = usage;
        create_info.samples = VK_SAMPLE_COUNT_1_BIT;

        auto indices = renderer::find_queue_families(physical_device).create_set();
        std::vector<uint32_t> unique_indices(indices.begin(), indices.end());
        if (unique_indices.size() > 1) {
            create_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
            create_info.pQueueFamilyIndices = unique_indices.data();
            create_info.queueFamilyIndexCount = unique_indices.size();
        } else {
            create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        _allocator.alloc(create_info, memory_usage, image, allocation);
    }

    static void get_stage_and_mask(VkImageLayout layout, VkPipelineStageFlags& stage,
                                   VkAccessFlags& access_mask) {
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
            stage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            access_mask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            stage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
            access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            stage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
            access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            throw std::runtime_error("unimplemented/unsupported image layout!");
        }
    }

    void transition_image_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout,
                                 VkImageAspectFlags image_aspect) {
        VkImageMemoryBarrier barrier;
        util::zero(barrier);
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = image_aspect;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        VkPipelineStageFlags source_stage, destination_stage;
        get_stage_and_mask(old_layout, source_stage, barrier.srcAccessMask);
        get_stage_and_mask(new_layout, destination_stage, barrier.dstAccessMask);
        auto cmdbuffer = renderer::create_single_time_command_buffer();
        cmdbuffer->begin();
        vkCmdPipelineBarrier(cmdbuffer->get(), source_stage, destination_stage, 0, 0, nullptr, 0,
                             nullptr, 1, &barrier);
        cmdbuffer->end();
        cmdbuffer->submit();
    }

    static void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width,
                                     uint32_t height) {
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
        vkCmdCopyBufferToImage(cmdbuffer->get(), buffer, image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        cmdbuffer->end();
        cmdbuffer->submit();
    }

    void image::update_dependent_imgui_textures() {
        for (texture* tex : this->m_dependents) {
            tex->update_imgui_texture();
        }
    }

    bool image2d::load_image(const fs::path& path, image_data& data) {
        std::string string_path = path.string();
        if (path.extension() == ".ktx") {
            ktxTexture* ktx_data;
            if (ktxTexture_CreateFromNamedFile(string_path.c_str(),
                                               KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                               &ktx_data) != KTX_SUCCESS) {
                return false;
            }

            size_t offset;
            if (ktxTexture_GetImageOffset(ktx_data, 0, 0, 0, &offset) != KTX_SUCCESS) {
                return false;
            }

            data.width = (int32_t)ktx_data->baseWidth;
            data.height = (int32_t)ktx_data->baseHeight;
            data.channels = (int32_t)(ktxTexture_GetRowPitch(ktx_data, 0) / ktx_data->baseWidth);

            size_t data_size = (size_t)data.width * data.height * data.channels;
            data.data.resize(data_size);

            uint8_t* raw_data = ktxTexture_GetData(ktx_data) + offset;
            if (raw_data != nullptr) {
                memcpy(data.data.data(), raw_data, data_size);
            }

            ktxTexture_Destroy(ktx_data);
        } else {
            uint8_t* raw_data =
                stbi_load(string_path.c_str(), &data.width, &data.height, &data.channels, 0);
            if (!raw_data) {
                return false;
            }
            size_t byte_count = (size_t)data.width * data.height * data.channels;
            data.data.resize(byte_count);
            // probably shouldnt use memcpy
            memcpy(data.data.data(), raw_data, byte_count * sizeof(uint8_t));
            stbi_image_free(raw_data);
        }
        return true;
    }

    ref<image2d> image2d::from_file(const fs::path& path) {
        ref<image2d> created_image;
        image_data data;
        if (load_image(path, data)) {
            created_image = ref<image2d>::create(data);
        }
        return created_image;
    }

    image2d::image2d(const image_data& data) {
        renderer::add_ref();
        this->init_basic();
        this->m_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        this->create_image_from_data(data);
        this->create_view();
    }

    image2d::image2d(VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usage,
                 VkImageAspectFlags aspect) {
        // only use this constructor for internal things such as depth buffering
        renderer::add_ref();
        this->init_basic();
        this->m_format = format;
        this->m_aspect = aspect;
        create_image(this->m_allocator, width, height, 1, this->m_format, VK_IMAGE_TILING_OPTIMAL,
                     usage, VMA_MEMORY_USAGE_GPU_ONLY, this->m_image, this->m_allocation);
        this->transition(VK_IMAGE_LAYOUT_GENERAL);
        this->create_view();
    }

    image2d::~image2d() {
        VkDevice device = renderer::get_device();
        vkDestroyImageView(device, this->m_view, nullptr);
        this->m_allocator.free(this->m_image, this->m_allocation);
        renderer::remove_ref();
    }

    void image2d::transition(VkImageLayout new_layout) {
        transition_image_layout(this->m_image, this->m_layout, new_layout, this->m_aspect);
        this->m_layout = new_layout;
        this->update_dependent_imgui_textures();
    }

    void image2d::init_basic() {
        this->m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        this->m_allocator.set_source("image");
    }

    void image2d::create_image_from_data(const image_data& data) {
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
        VmaAllocation staging_allocation;
        size_t total_size = (size_t)data.width * data.height * data.channels;
        create_buffer(this->m_allocator, total_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU, staging_buffer, staging_allocation);

        void* gpu_data = this->m_allocator.map(staging_allocation);
        memcpy(gpu_data, data.data.data(), total_size);
        this->m_allocator.unmap(staging_allocation);
        create_image(this->m_allocator, data.width, data.height, 1, this->m_format,
                     VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VMA_MEMORY_USAGE_GPU_ONLY, this->m_image, this->m_allocation);

        this->transition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copy_buffer_to_image(staging_buffer, this->m_image, data.width, data.height);
        this->transition(VK_IMAGE_LAYOUT_GENERAL);

        this->m_allocator.free(staging_buffer, staging_allocation);
    }

    void image2d::create_view() {
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
} // namespace vkrollercoaster