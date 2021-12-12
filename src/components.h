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
#include "model.h"
#include "light.h"
#include "script.h"
#include "scene.h"
namespace vkrollercoaster {
    struct tag_component {
        tag_component() = default;
        std::string tag;
    };
    struct transform_component {
        transform_component() = default;
        glm::vec3 translation = glm::vec3(0.f);
        glm::vec3 rotation = glm::vec3(0.f);
        glm::vec3 scale = glm::vec3(1.f);
    };
    struct model_component {
        model_component() = default;
        ref<model> data;
        // todo: animation data (when skinning)
    };
    struct camera_component {
        camera_component() = default;
        // in degrees
        float fov = 45.f;
        bool primary = false;
        glm::vec3 up = glm::vec3(0.f, 1.f, 0.f);
    };
    struct light_component {
        light_component() = default;
        ref<light> data;
        // literally nothing else.
    };
    struct script_component {
        script_component() = default;

        entity parent;
        std::vector<ref<script>> scripts;

        template<typename T, typename... Args> void bind(Args&&... args) {
            static_assert(std::is_base_of_v<script, T>, "the given type is not derived from \"script!\"");

            ref<script> _script = ref<T>::create(std::forward<Args>(args)...);
            _script->m_parent = this->parent;
            _script->on_added();
            _script->on_enable();

            this->scripts.push_back(_script);
        }
    };

    //==== scene::on_component_added overloads ====
    template<> inline void scene::on_component_added<script_component>(entity& ent, script_component& component) {
        component.parent = ent;
    }
}