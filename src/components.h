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
namespace vkrollercoaster {
    struct tag_component {
        std::string tag;
    };
    struct transform_component {
        glm::vec3 translation = glm::vec3(0.f);
        glm::vec3 rotation = glm::vec3(0.f);
        glm::vec3 scale = glm::vec3(1.f);
        transform_component() = default;
        glm::mat4 matrix() const {
            glm::mat4 rotation_matrix = glm::toMat4(glm::quat(this->rotation));
            return
                glm::translate(glm::mat4(1.f), this->translation) *
                rotation_matrix *
                glm::scale(glm::mat4(1.f), this->scale);
        }
    };
    struct model_component {
        ref<model> data;
        // todo: animation data (when skinning)
    };
}