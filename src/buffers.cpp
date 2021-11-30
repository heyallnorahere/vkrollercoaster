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
        renderer::add_ref();
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
        renderer::remove_ref();
    }
    void vertex_buffer::bind(ref<command_buffer> cmdbuffer, uint32_t slot) {
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdbuffer->get(), slot, 1, &this->m_buffer, &offset);
    }
    index_buffer::index_buffer(const uint32_t* data, size_t index_count) {
        this->m_index_count = index_count;
        size_t size = index_count * sizeof(uint32_t);
        renderer::add_ref();
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
        renderer::remove_ref();
    }
    void index_buffer::bind(ref<command_buffer> cmdbuffer) {
        vkCmdBindIndexBuffer(cmdbuffer->get(), this->m_buffer, 0, VK_INDEX_TYPE_UINT32);
    }
    ref<uniform_buffer> uniform_buffer::from_shader_data(ref<shader> _shader, uint32_t set, uint32_t binding) {
        auto& reflection_data = _shader->get_reflection_data();
        if (reflection_data.resources.find(set) == reflection_data.resources.end()) {
            throw std::runtime_error("the specified set does not exist!");
        }
        auto& descriptor_set = reflection_data.resources[set];
        if (descriptor_set.find(binding) == descriptor_set.end()) {
            throw std::runtime_error("the specified binding does not exist!");
        }
        auto& binding_data = descriptor_set[binding];
        if (binding_data.resource_type != shader_resource_type::uniformbuffer) {
            throw std::runtime_error("the specified binding is not a uniform buffer!");
        }
        size_t size = reflection_data.types[binding_data.type].size;
        return ref<uniform_buffer>::create(set, binding, size);
    }
    uniform_buffer::uniform_buffer(uint32_t set, uint32_t binding, size_t size) {
        renderer::add_ref();
        this->m_set = set;
        this->m_binding = binding;
        this->m_size = size;
        create_buffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            this->m_buffer, this->m_memory);
    }
    uniform_buffer::~uniform_buffer() {
        for (auto _pipeline : this->m_bound_pipelines) {
            auto& set_data = _pipeline->m_bound_buffers[this->m_set];
            if (set_data.find(this->m_binding) == set_data.end()) {
                continue;
            }
            if (set_data[this->m_binding].object == this) {
                set_data.erase(this->m_binding);
            }
        }
        VkDevice device = renderer::get_device();
        vkDestroyBuffer(device, this->m_buffer, nullptr);
        vkFreeMemory(device, this->m_memory, nullptr);
        renderer::remove_ref();
    }
    void uniform_buffer::bind(ref<pipeline> _pipeline) {
        auto& descriptor_sets = _pipeline->m_descriptor_sets;
        if (descriptor_sets.find(this->m_set) == descriptor_sets.end()) {
            throw std::runtime_error("attempted to bind to a nonexistent descriptor set!");
        }
        VkDescriptorBufferInfo buffer_info;
        util::zero(buffer_info);
        buffer_info.buffer = this->m_buffer;
        buffer_info.range = this->m_size;
        buffer_info.offset = 0;
        std::vector<VkWriteDescriptorSet> descriptor_writes;
        for (VkDescriptorSet set : descriptor_sets[this->m_set].sets) {
            VkWriteDescriptorSet descriptor_write;
            util::zero(descriptor_write);
            descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_write.dstSet = set;
            descriptor_write.dstBinding = this->m_binding;
            descriptor_write.dstArrayElement = 0;
            descriptor_write.pBufferInfo = &buffer_info;
            descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor_write.descriptorCount = 1;
            descriptor_writes.push_back(descriptor_write);
        }
        VkDevice device = renderer::get_device();
        vkUpdateDescriptorSets(device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
        if (this->m_bound_pipelines.find(_pipeline.raw()) == this->m_bound_pipelines.end()) {
            this->m_bound_pipelines.insert(_pipeline.raw());
        }
        if (_pipeline->m_bound_buffers[this->m_set][this->m_binding].object != this) {
            pipeline::bound_buffer_desc desc;
            desc.object = this;
            desc.type = pipeline::buffer_type::ubo;
            _pipeline->m_bound_buffers[this->m_set][this->m_binding] = desc;
        }
    }
    void uniform_buffer::set_data(const void* data, size_t size, size_t offset) {
        VkDevice device = renderer::get_device();
        if (size + offset > this->m_size) {
            throw std::runtime_error("attempted to map memory outside the buffer's limits");
        }
        void* gpu_data;
        vkMapMemory(device, this->m_memory, offset, size, 0, &gpu_data);
        memcpy(gpu_data, data, size);
        vkUnmapMemory(device, this->m_memory);
    }
}