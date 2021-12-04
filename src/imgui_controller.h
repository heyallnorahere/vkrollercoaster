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
    class menu : public ref_counted {
    public:
        menu() = default;
        virtual ~menu() = default;
        virtual std::string get_title() = 0;
        virtual void update() = 0;
        bool& open() { return this->m_open; }
    protected:
        bool m_open = true;
    };
    class imgui_controller : public ref_counted {
    public:
        imgui_controller(ref<swapchain> _swapchain);
        ~imgui_controller();
        imgui_controller(const imgui_controller&) = delete;
        imgui_controller& operator=(const imgui_controller&) = delete;
        void new_frame();
        void update_menus();
        void render(ref<command_buffer> cmdbuffer);
    private:
        ref<swapchain> m_swapchain;
        std::vector<ref<menu>> m_menus;
    };
}