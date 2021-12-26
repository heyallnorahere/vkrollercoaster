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
#include "scene_serializer.h"
#include "components.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace glm {
    void to_json(json& data, const glm::vec3& vec) {
        data["x"] = vec.x;
        data["y"] = vec.y;
        data["z"] = vec.z;
    }
    void to_json(json& data, const glm::vec2& vec) {
        data["x"] = vec.x;
        data["y"] = vec.y;
    }
}

namespace vkrollercoaster {
    void to_json(json& data, const tag_component& tag) {
        data["tag"] = tag.tag;
    }

    void to_json(json& data, const transform_component& transform) {
        data["translation"] = transform.translation;
        data["rotation"] = transform.rotation;
        data["scale"] = transform.scale;
    }

    void to_json(json& data, const vertex& v) {
        data["position"] = v.position;
        data["normal"] = v.normal;
        data["uv"] = v.uv;
        data["tangent"] = v.tangent;
    }
    void to_json(json& data, const model::mesh& mesh) {
        data["index_offset"] = mesh.index_offset;
        data["index_count"] = mesh.index_count;
        data["material_index"] = mesh.material_index;
    }
    void to_json(json& data, const model_component& component) {
        ref<model> model_data = component.data;
        auto source = model_data->get_source();
        if (source) {
            data["type"] = "file";

            fs::path filepath = source->get_path();
            data["path"] = filepath.relative_path().string();
        } else {
            data["type"] = "data";

            data["vertices"] = model_data->get_vertices();
            data["indices"] = model_data->get_indices();
            data["meshes"] = model_data->get_meshes();
            // todo: materials
        }
    }
    
    void to_json(json& data, const camera_component& camera) {
        data["fov"] = camera.fov;
        data["primary"] = camera.primary;
        data["up"] = camera.up;
    }

    scene_serializer::scene_serializer(ref<scene> _scene) { this->m_scene = _scene; }
    void scene_serializer::serialize(const fs::path& path) {
        json scene_data;

        json entities;
        this->m_scene->for_each([&](entity ent) {
            json entity_data;
            entity_data["uuid"] = 0; // no uuid for now

            json component_data;

            if (ent.has_component<tag_component>()) {
                component_data["tag"] = ent.get_component<tag_component>();
            }

            if (ent.has_component<transform_component>()) {
                component_data["transform"] = ent.get_component<transform_component>();
            }

            if (ent.has_component<model_component>()) {
                component_data["model"] = ent.get_component<model_component>();
            }

            if (ent.has_component<camera_component>()) {
                component_data["camera"] = ent.get_component<camera_component>();
            }

            // todo: light, script, and track segment components

            entity_data["components"] = component_data;
            entities.push_back(component_data);
        });
        scene_data["entities"] = entities;

        std::ofstream file(path);
        file << scene_data.dump(4) << std::flush;
        file.close();
    }
}