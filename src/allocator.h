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
namespace vkrollercoaster {
    class allocator {
    public:
        static void init();
        static void shutdown();

        allocator();
        ~allocator();

        allocator(const allocator&) = delete;
        allocator& operator=(const allocator&) = delete;

        void set_source(const std::string& source);

        // images
        void alloc(const VkImageCreateInfo& create_info, VmaMemoryUsage usage, VkImage& image,
                   VmaAllocation& allocation) const;
        void free(VkImage image, VmaAllocation allocation) const;

        // buffers
        void alloc(const VkBufferCreateInfo& create_info, VmaMemoryUsage usage, VkBuffer& buffer,
                   VmaAllocation& allocation) const;
        void free(VkBuffer buffer, VmaAllocation allocation) const;

        // mapping memory
        void* map(VmaAllocation allocation) const;
        void unmap(VmaAllocation allocation) const;

    private:
        std::string m_source;
    };
} // namespace vkrollercoaster