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
    void window::init() {
        if (!glfwInit()) {
            throw std::runtime_error("could not initialize glfw!");
        }
    }
    void window::shutdown() {
        glfwTerminate();
    }
    void window::poll() {
        glfwPollEvents();
    }
    window::window(int32_t width, int32_t height, const std::string& title) {
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
    }
    bool window::should_close() const {
        return glfwWindowShouldClose(this->m_window);
    }
}