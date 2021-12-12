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
#include "allocator.h"
#include "renderer.h"
#include "util.h"
namespace vkrollercoaster {
    static struct {
        VmaAllocator allocator = nullptr;
        bool should_shutdown = false;
        uint64_t allocator_count = 0;
    } allocator_data;

    void allocator::init() {
        renderer::add_ref();

        VmaAllocatorCreateInfo create_info;
        util::zero(create_info);

        create_info.vulkanApiVersion = renderer::get_vulkan_version();
        create_info.instance = renderer::get_instance();
        create_info.physicalDevice = renderer::get_physical_device();
        create_info.device = renderer::get_device();

        if (vmaCreateAllocator(&create_info, &allocator_data.allocator) != VK_SUCCESS) {
            throw std::runtime_error("could not create allocator!");
        }
    }

    static void shutdown_allocator() {
        vmaDestroyAllocator(allocator_data.allocator);

        renderer::remove_ref();
    }
    void allocator::shutdown() {
        allocator_data.should_shutdown = true;
        if (allocator_data.allocator_count == 0) {
            shutdown_allocator();
        }
    }

    allocator::allocator() {
        allocator_data.allocator_count++;
        this->m_source = "unknown";
    }

    allocator::~allocator() {
        allocator_data.allocator_count--;

        if (allocator_data.allocator_count == 0 && allocator_data.should_shutdown) {
            shutdown_allocator();
        }
    }

    void allocator::set_source(const std::string& source) {
        this->m_source = source;
    }

    void allocator::alloc(const VkImageCreateInfo& create_info, VmaMemoryUsage usage, VkImage& image, VmaAllocation& allocation) const {
        VmaAllocationCreateInfo alloc_info;
        util::zero(alloc_info);
        alloc_info.usage = usage;

        if (vmaCreateImage(allocator_data.allocator, &create_info, &alloc_info, &image, &allocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error(this->m_source + ": could not create image!");
        }
    }

    void allocator::free(VkImage image, VmaAllocation allocation) const {
        vmaDestroyImage(allocator_data.allocator, image, allocation);
    }

    void allocator::alloc(const VkBufferCreateInfo& create_info, VmaMemoryUsage usage, VkBuffer& buffer, VmaAllocation& allocation) const {
        VmaAllocationCreateInfo alloc_info;
        util::zero(alloc_info);
        alloc_info.usage = usage;

        if (vmaCreateBuffer(allocator_data.allocator, &create_info, &alloc_info, &buffer, &allocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error(this->m_source + ": could not create buffer!");
        }
    }

    void allocator::free(VkBuffer buffer, VmaAllocation allocation) const {
        vmaDestroyBuffer(allocator_data.allocator, buffer, allocation);
    }

    void* allocator::map(VmaAllocation allocation) const {
        void* gpu_data;
        if (vmaMapMemory(allocator_data.allocator, allocation, &gpu_data) != VK_SUCCESS) {
            throw std::runtime_error(this->m_source + ": could not map memory!");
        }
        return gpu_data;
    }

    void allocator::unmap(VmaAllocation allocation) const {
        vmaUnmapMemory(allocator_data.allocator, allocation);
    }
}