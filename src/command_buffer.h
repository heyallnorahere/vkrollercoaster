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
#include "render_target.h"
#include "shader.h"
namespace vkrollercoaster {
    class renderer;
    struct internal_cmdbuffer_data;
    class command_buffer : public ref_counted {
    public:
        ~command_buffer();
        command_buffer(const command_buffer&) = delete;
        command_buffer& operator=(const command_buffer&) = delete;
        void begin();
        void end();
        void submit();
        void wait();
        void reset();
        void begin_render_pass(ref<render_target> target, const glm::vec4& clear_color);
        void end_render_pass();
        VkCommandBuffer get() { return this->m_buffer; }
        ref<render_target> get_current_render_target() { return this->m_current_render_target; }
        bool recording() { return this->m_recording; }

    private:
        command_buffer(VkCommandPool command_pool, VkQueue queue, bool single_time, bool render);
        
        ref<render_target> m_current_render_target;
        internal_cmdbuffer_data* m_internal_data;

        VkCommandPool m_pool;
        VkQueue m_queue;
        VkCommandBuffer m_buffer;

        bool m_single_time, m_render, m_recorded, m_recording;
        friend class renderer;
    };
} // namespace vkrollercoaster