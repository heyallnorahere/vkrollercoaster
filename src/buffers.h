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
#include "pipeline.h"
#include "shader.h"
namespace vkrollercoaster {
#ifdef EXPOSE_RENDERER_INTERNALS
    uint32_t find_memory_type(uint32_t filter, VkMemoryPropertyFlags properties);
    void create_buffer(size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory);
    void copy_buffer(VkBuffer src, VkBuffer dest, size_t size, size_t src_offset = 0, size_t dest_offset = 0);
#endif
    class vertex_buffer : public ref_counted {
    public:
        template<typename T> vertex_buffer(const std::vector<T>& data) : vertex_buffer(data.data(), data.size() * sizeof(T)) { }
        vertex_buffer(const void* data, size_t size);
        ~vertex_buffer();
        vertex_buffer(const vertex_buffer&) = delete;
        vertex_buffer& operator=(const vertex_buffer&) = delete;
        void bind(ref<command_buffer> cmdbuffer, uint32_t slot = 0);
    private:
        VkBuffer m_buffer;
        VkDeviceMemory m_memory;
    };
    class index_buffer : public ref_counted {
    public:
        index_buffer(const std::vector<uint32_t>& data) : index_buffer(data.data(), data.size()) { }
        index_buffer(const uint32_t* data, size_t index_count);
        ~index_buffer();
        index_buffer(const index_buffer&) = delete;
        index_buffer& operator=(const index_buffer&) = delete;
        void bind(ref<command_buffer> cmdbuffer);
        size_t get_index_count() { return this->m_index_count; }
    private:
        VkBuffer m_buffer;
        VkDeviceMemory m_memory;
        size_t m_index_count;
    };
    class uniform_buffer : public ref_counted {
    public:
        static ref<uniform_buffer> from_shader_data(ref<shader> _shader, uint32_t set, uint32_t binding);
        uniform_buffer(uint32_t set, uint32_t binding, size_t size);
        ~uniform_buffer();
        uniform_buffer(const uniform_buffer&) = delete;
        uniform_buffer& operator=(const uniform_buffer&) = delete;
        void bind(ref<pipeline> _pipeline, size_t current_image);
        template<typename T> void set_data(const T& data, size_t offset = 0) {
            this->set_data(&data, sizeof(T), offset);
        }
        void set_data(const void* data, size_t size, size_t offset = 0);
    private:
        VkBuffer m_buffer;
        VkDeviceMemory m_memory;
        uint32_t m_set, m_binding;
        size_t m_size;
    };
}