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
#include "renderer.h"
#include "window.h"
#include "swapchain.h"
#include "shader.h"
using namespace vkrollercoaster;
int32_t main(int32_t argc, const char** argv) {
    window::init();
    auto application_window = std::make_shared<window>(1600, 900, "vkrollercoaster");
    renderer::init(application_window);
    auto swap_chain = std::make_shared<swapchain>();
    auto testshader = std::make_shared<shader>("assets/shaders/testshader.glsl");
    while (!application_window->should_close()) {
        // todo: render
        window::poll();
    }
    renderer::shutdown();
    window::shutdown();
    return 0;
}