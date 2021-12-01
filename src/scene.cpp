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
#include "scene.h"
#include "components.h"
namespace vkrollercoaster {
    void scene::update() {
        std::unordered_map<ref<light>, std::vector<entity>> lights;
        for (entity ent : this->view<transform_component, light_component>()) {
            ref<light> _light = ent.get_component<light_component>().data;
            lights[_light].push_back(ent);
        }
        for (const auto& [_light, entities] : lights) {
            _light->update_buffers(entities);
        }
    }
    entity scene::create()  {
        entt::entity id = this->m_registry.create();
        entity ent(id, this);
        ent.add_component<transform_component>();
        return ent;
    }
    entity scene::create(const std::string& tag) {
        entity ent = this->create();
        ent.add_component<tag_component>().tag = tag;
        return ent;
    }
    std::vector<entity> scene::find_tag(const std::string& tag) {
        std::vector<entity> entities;
        std::vector<entity> tag_view = this->view<tag_component>();
        for (entity ent : tag_view) {
            const auto& entity_tag = ent.get_component<tag_component>();
            if (entity_tag.tag == tag) {
                entities.push_back(ent);
            }
        }
        return entities;
    }
}