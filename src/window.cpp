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
#include "window.h"
namespace vkrollercoaster {
    static struct {
        uint32_t window_count = 0;
        bool initialized = false;
        bool should_shutdown = false;
    } window_data;
    void window::init() {
        if (!glfwInit()) {
            throw std::runtime_error("could not initialize glfw!");
        }
        window_data.initialized = true;
    }
    static void shutdown_glfw() {
        glfwTerminate();
    }
    void window::shutdown() {
        window_data.should_shutdown = true;
        if (window_data.window_count == 0) {
            shutdown_glfw();
        }
    }
    void window::poll() {
        glfwPollEvents();
    }
    window::window(int32_t width, int32_t height, const std::string& title) {
        if (!window_data.initialized) {
            throw std::runtime_error("glfw has not been initialized!");
        } else if (window_data.should_shutdown) {
            throw std::runtime_error("glfw has already been shut down!");
        }
        window_data.window_count++;
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        this->m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        if (!this->m_window) {
            const char* desc;
            int32_t error_code = glfwGetError(&desc);
            throw std::runtime_error("glfw error " + std::to_string(error_code) + ": " + desc);
        }
    }
    window::~window() {
        glfwDestroyWindow(this->m_window);
        window_data.window_count--;
        if (window_data.window_count == 0 && window_data.should_shutdown) {
            shutdown_glfw();
        }
    }
    bool window::should_close() const {
        return glfwWindowShouldClose(this->m_window);
    }
}