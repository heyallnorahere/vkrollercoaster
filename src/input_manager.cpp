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
#include "input_manager.h"
namespace vkrollercoaster {
    struct window_input_data {
        std::list<input_manager*> ims;
        GLFWkeyfun previous_key_callback;
    };
    static struct {
        std::unordered_map<GLFWwindow*, window_input_data> window_map;
    } input_data;

    input_manager::input_manager(ref<window> _window) {
        this->m_window = _window;

        GLFWwindow* glfw_window = this->m_window->get();
        if (input_data.window_map.find(glfw_window) == input_data.window_map.end()) {
            input_data.window_map.insert(std::make_pair(glfw_window, window_input_data()));


            auto wid = &input_data.window_map[glfw_window];
            glfwSetCursorPosCallback(glfw_window, cursor_pos_callback);
            GLFWkeyfun previous_callback = glfwSetKeyCallback(glfw_window, key_callback);
            if (previous_callback != key_callback) {
                wid->previous_key_callback = previous_callback;
            }
        }
        input_data.window_map[glfw_window].ims.push_back(this);
    }

    input_manager::~input_manager() {
        GLFWwindow* glfw_window = this->m_window->get();
        input_data.window_map[glfw_window].ims.remove(this);

        if (input_data.window_map[glfw_window].ims.empty()) {
            input_data.window_map.erase(glfw_window);
        }
    }

    void input_manager::update() {
        input_state last = this->m_current;
        this->m_current = this->m_writing;
        this->m_writing = input_state();
        this->m_last_mouse = last.mouse;

        if (glm::length(this->m_current.mouse) < 0.0001f) {
            this->m_current.mouse = this->m_last_mouse;
        }

        std::vector<int32_t> keys;
        for (const auto& [key, _] : last.keys) {
            keys.push_back(key);
        }
        for (const auto& [key, _] : this->m_current.keys) {
            keys.push_back(key);
        }

        for (int32_t key : keys) {
            key_state last_key;
            if (last.keys.find(key) != last.keys.end()) {
                last_key = last.keys[key];
            }

            if (this->m_current.keys.find(key) == this->m_current.keys.end()) {
                this->m_current.keys.insert(std::make_pair(key, last_key));
            }
            auto& current_key = this->m_current.keys[key];

            current_key.down = current_key.held && !last_key.held;
            current_key.up = !current_key.held && last_key.held;
        }
    }

    void input_manager::enable_cursor() {
        GLFWwindow* glfw_window = this->m_window->get();
        glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    void input_manager::disable_cursor() {
        GLFWwindow* glfw_window = this->m_window->get();
        glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    bool input_manager::is_cursor_enabled() {
        GLFWwindow* glfw_window = this->m_window->get();
        int32_t mode = glfwGetInputMode(glfw_window, GLFW_CURSOR);
        return mode != GLFW_CURSOR_DISABLED;
    }

    input_manager::key_state input_manager::get_key(int32_t key) {
        key_state state;
        if (this->m_current.keys.find(key) != this->m_current.keys.end()) {
            state = this->m_current.keys[key];
        }
        return state;
    }

    void input_manager::cursor_pos_callback(GLFWwindow* glfw_window, double x, double y) {
        for (input_manager* im : input_data.window_map[glfw_window].ims) {
            im->m_writing.mouse = glm::vec2((float)x, (float)y);
        }
    }

    void input_manager::key_callback(GLFWwindow* glfw_window, int32_t key, int32_t scancode, int32_t action, int32_t mods) {
        // call any previous callbacks so we don't lose functionality
        GLFWkeyfun previous_callback = input_data.window_map[glfw_window].previous_key_callback;
        if (previous_callback != nullptr) {
            previous_callback(glfw_window, key, scancode, action, mods);
        }

        for (input_manager* im : input_data.window_map[glfw_window].ims) {
            if (im->m_writing.keys.find(key) == im->m_writing.keys.end()) {
                im->m_writing.keys.insert(std::make_pair(key, key_state()));
            }

            auto& state = im->m_writing.keys[key];
            state.held = (action != GLFW_RELEASE);
            state.mods |= mods;
        }
    }
}