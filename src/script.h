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
#include "scene.h"
namespace vkrollercoaster {
    struct script_component;
    class script : public ref_counted {
    public:
        script() = default;
        virtual ~script() = default;
        virtual void on_added() { }
        virtual void update() = 0;
        entity get_parent() { return this->m_parent; }
    protected:
        template<typename T> bool has_component() {
            return this->m_parent.has_component<T>();
        }
        template<typename T> T& get_component() {
            return this->m_parent.get_component<T>();
        }
        template<typename T, typename... Args> T& add_component(Args&&... args) {
            return this->m_parent.add_component<T>(std::forward<Args>(args)...);
        }
        template<typename T> void remove_component() {
            this->m_parent.remove_component<T>();
        }
        entity m_parent;
        friend struct script_component;
    };
};