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
#include "command_buffer.h"
namespace vkrollercoaster {
    class imgui_controller {
    public:
        imgui_controller(std::shared_ptr<swapchain> _swapchain);
        ~imgui_controller();
        void new_frame();
        void render(std::shared_ptr<command_buffer> cmdbuffer);
    private:
        std::shared_ptr<swapchain> m_swapchain;
    };
}