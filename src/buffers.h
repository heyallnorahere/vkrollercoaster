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
#include "command_buffer.h"
namespace vkrollercoaster {
#ifdef EXPOSE_RENDERER_INTERNALS
    uint32_t find_memory_type(uint32_t filter, VkMemoryPropertyFlags properties);
    void create_buffer(size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
    void copy_buffer(VkBuffer src, VkBuffer dest, size_t size, size_t src_offset = 0, size_t dest_offset = 0);
#endif
    class vertex_buffer {
    public:
        template<typename T> vertex_buffer(const std::vector<T>& data) : vertex_buffer(data.data(), data.size() * sizeof(T)) { }
        vertex_buffer(const void* data, size_t size);
        ~vertex_buffer();
        vertex_buffer(const vertex_buffer&) = delete;
        vertex_buffer& operator=(const vertex_buffer&) = delete;
        void bind(std::shared_ptr<command_buffer> cmdbuffer, uint32_t slot = 0);
    private:
        VkBuffer m_buffer;
        VkDeviceMemory m_memory;
    };
}