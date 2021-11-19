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
#include "swapchain.h"
#define EXPOSE_RENDERER_INTERNALS
#include "renderer.h"
namespace vkrollercoaster {
    swapchain::swapchain() {
        renderer::add_ref();
        this->m_window = renderer::get_window();
        int32_t width, height;
        this->m_window->get_size(&width, &height);
        this->create(width, height);
    }
    swapchain::~swapchain() {
        this->destroy();
        renderer::remove_ref();
    }
    void swapchain::reload() {
        // todo: recreate swapchain
    }
    void swapchain::create(int32_t width, int32_t height) {
        // todo: create swapchain
    }
    void swapchain::destroy() {
        // todo: cleanup swapchain
    }
}