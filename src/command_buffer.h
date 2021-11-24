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
#include "swapchain.h"
#include "shader.h"
namespace vkrollercoaster {
    class renderer;
    class command_buffer {
    public:
        ~command_buffer();
        command_buffer(const command_buffer&) = delete;
        command_buffer& operator=(const command_buffer&) = delete;
        void begin();
        void end();
        void submit();
        void reset();
        void begin_render_pass(std::shared_ptr<swapchain> swap_chain, const glm::vec4& clear_color, size_t image_index);
        void end_render_pass();
        VkCommandBuffer get() { return this->m_buffer; }
    private:
        command_buffer(VkCommandPool command_pool, VkQueue queue, bool single_time, bool render);
        VkCommandPool m_pool;
        VkQueue m_queue;
        VkCommandBuffer m_buffer;
        bool m_single_time, m_render;
        friend class renderer;
    };
}