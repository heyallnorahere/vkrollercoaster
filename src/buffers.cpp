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
#include "buffers.h"
#include "renderer.h"
#include "util.h"
namespace vkrollercoaster {
    uint32_t find_memory_type(uint32_t filter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memory_properties;
        VkPhysicalDevice physical_device = renderer::get_physical_device();
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
        for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
            if ((filter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        throw std::runtime_error("could not find suitable memory type!");
        return 0;
    }
    void create_buffer(size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory) {
        VkDevice device = renderer::get_device();
        VkBufferCreateInfo create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        create_info.size = size;
        create_info.usage = usage;
        create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &create_info, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("could not create GPU buffer!");
        }
        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(device, buffer, &requirements);
        VkMemoryAllocateInfo alloc_info;
        util::zero(alloc_info);
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = requirements.size;
        alloc_info.memoryTypeIndex = find_memory_type(requirements.memoryTypeBits, properties);
        // todo: use vma or something
        if (vkAllocateMemory(device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
            throw std::runtime_error("could not allocate memory on the GPU!");
        }
        vkBindBufferMemory(device, buffer, memory, 0);
    }
    void copy_buffer(VkBuffer src, VkBuffer dest, size_t size, size_t src_offset, size_t dest_offset) {
        VkBufferCopy region;
        util::zero(region);
        region.srcOffset = src_offset;
        region.dstOffset = dest_offset;
        region.size = size;
        auto cmdbuffer = renderer::create_single_time_command_buffer();
        cmdbuffer->begin();
        vkCmdCopyBuffer(cmdbuffer->get(), src, dest, 1, &region);
        cmdbuffer->end();
        cmdbuffer->submit();
    }
    vertex_buffer::vertex_buffer(const void* data, size_t size) {
        VkBuffer staging_buffer;
        VkDeviceMemory staging_memory;
        create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer, staging_memory);
        void* gpu_data;
        VkDevice device = renderer::get_device();
        vkMapMemory(device, staging_memory, 0, size, 0, &gpu_data);
        memcpy(gpu_data, data, size);
        vkUnmapMemory(device, staging_memory);
        create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            this->m_buffer, this->m_memory);
        copy_buffer(staging_buffer, this->m_buffer, size);
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_memory, nullptr);
    }
    vertex_buffer::~vertex_buffer() {
        VkDevice device = renderer::get_device();
        vkDestroyBuffer(device, this->m_buffer, nullptr);
        vkFreeMemory(device, this->m_memory, nullptr);
    }
    void vertex_buffer::bind(std::shared_ptr<command_buffer> cmdbuffer, uint32_t slot) {
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdbuffer->get(), slot, 1, &this->m_buffer, &offset);
    }
    index_buffer::index_buffer(const uint32_t* data, size_t index_count) {
        this->m_index_count = index_count;
        size_t size = index_count * sizeof(uint32_t);
        VkBuffer staging_buffer;
        VkDeviceMemory staging_memory;
        create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging_buffer, staging_memory);
        void* gpu_data;
        VkDevice device = renderer::get_device();
        vkMapMemory(device, staging_memory, 0, size, 0, &gpu_data);
        memcpy(gpu_data, data, size);
        vkUnmapMemory(device, staging_memory);
        create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            this->m_buffer, this->m_memory);
        copy_buffer(staging_buffer, this->m_buffer, size);
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_memory, nullptr);
    }
    index_buffer::~index_buffer() {
        VkDevice device = renderer::get_device();
        vkDestroyBuffer(device, this->m_buffer, nullptr);
        vkFreeMemory(device, this->m_memory, nullptr);
    }
    void index_buffer::bind(std::shared_ptr<command_buffer> cmdbuffer) {
        vkCmdBindIndexBuffer(cmdbuffer->get(), this->m_buffer, 0, VK_INDEX_TYPE_UINT32);
    }
}