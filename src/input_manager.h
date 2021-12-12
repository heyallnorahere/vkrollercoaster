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
#include "window.h"
namespace vkrollercoaster {
    class input_manager : public ref_counted {
    public:
        struct key_state {
            bool held = false;
            bool down = false;
            bool up = false;
            int32_t mods = 0;
        };

        input_manager(ref<window> _window);
        ~input_manager();
        input_manager(const input_manager&) = delete;
        input_manager& operator=(const input_manager&) = delete;

        void update();
        void enable_cursor();
        void disable_cursor();
        bool is_cursor_enabled();
        key_state get_key(int32_t key);
        glm::vec2 get_mouse_offset() { return this->m_current.mouse - this->m_last_mouse; }
        glm::vec2 get_mouse() { return this->m_current.mouse; }

    private:
        struct input_state {
            std::map<int32_t, key_state> keys;
            glm::vec2 mouse = glm::vec2(0.f);
        };

        ref<window> m_window;
        input_state m_current, m_writing;
        glm::vec2 m_last_mouse;

        static void cursor_pos_callback(GLFWwindow* glfw_window, double x, double y);
        static void key_callback(GLFWwindow* glfw_window, int32_t key, int32_t scancode,
                                 int32_t action, int32_t mods);
    };
} // namespace vkrollercoaster