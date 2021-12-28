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
#include "menus.h"
#include "../application.h"
#include "../components.h"
#include "scene_serializer.h"
#include "../imgui_extensions.h"
namespace vkrollercoaster {
    static void attenuation_editor(attenuation_settings& attenuation) {
        // based off of https://wiki.ogre3d.org/Light+Attenuation+Shortcut
        if (ImGui::CollapsingHeader("Attenuation")) {
            ImGui::Indent();
            ImGui::InputFloat("Target distance", &attenuation.target_distance);

            // constant
            ImGui::Checkbox("##edit-constant", &attenuation.constant.edit);
            ImGui::SameLine();
            ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;
            if (!attenuation.constant.edit) {
                flags |= ImGuiInputTextFlags_ReadOnly;
                attenuation.constant.value = 1.f;
            }
            ImGui::InputFloat("Constant", &attenuation.constant.value, 0.f, 0.f, "%.3f", flags);

            // linear
            ImGui::Checkbox("##edit-linear", &attenuation.linear.edit);
            ImGui::SameLine();
            flags = ImGuiInputTextFlags_None;
            if (!attenuation.linear.edit) {
                flags |= ImGuiInputTextFlags_ReadOnly;
                attenuation.linear.value = 4.5f / attenuation.target_distance;
            }
            ImGui::InputFloat("Linear", &attenuation.linear.value, 0.f, 0.f, "%.3f", flags);

            // quadratic
            ImGui::Checkbox("##edit-quadratic", &attenuation.quadratic.edit);
            ImGui::SameLine();
            flags = ImGuiInputTextFlags_None;
            if (!attenuation.quadratic.edit) {
                flags |= ImGuiInputTextFlags_ReadOnly;
                attenuation.quadratic.value = 75.f / glm::pow(attenuation.target_distance, 2.f);
            }
            ImGui::InputFloat("Quadratic", &attenuation.quadratic.value, 0.f, 0.f, "%.3f", flags);

            ImGui::Unindent();
        }
    }

    struct type_name_pair {
        light_type type;
        std::string name;
    };

    struct spotlight_creation_data {
        glm::vec3 direction = glm::vec3(0.f, -1.f, 0.f);
        float cutoff_angle = 12.5f;
        float outer_cutoff_angle = 17.5f;
    };

    struct light_creation_data {
        // this is it for now
        spotlight_creation_data spotlight_data;
    };

    static void light_editor(entity ent) {
        if (ent.has_component<light_component>()) {
            // if a light component exists on the passed entity, show fields for editing
            ref<light> _light = ent.get_component<light_component>().data;

            ImGui::ColorEdit3("Diffuse color", &_light->diffuse_color().x);
            ImGui::ColorEdit3("Ambient color", &_light->ambient_color().x);
            ImGui::ColorEdit3("Specular color", &_light->specular_color().x);

            switch (_light->get_type()) {
            case light_type::point: {
                ref<point_light> _point_light = _light.as<point_light>();
                attenuation_editor(_point_light->attenuation());
            } break;
            case light_type::spotlight: {
                ref<spotlight> _spotlight = _light.as<spotlight>();
                ImGui::InputFloat3("Direction", &_spotlight->direction().x);

                float angle = glm::degrees(glm::acos(_spotlight->cutoff()));
                ImGui::SliderFloat("Inner cutoff", &angle, 0.f, 45.f);
                _spotlight->cutoff() = glm::cos(glm::radians(angle));

                angle = glm::degrees(glm::acos(_spotlight->outer_cutoff()));
                ImGui::SliderFloat("Outer cutoff", &angle, 0.f, 45.f);
                _spotlight->outer_cutoff() = glm::cos(glm::radians(angle));

                attenuation_editor(_spotlight->attenuation());
            } break;
            }
            if (ImGui::Button("Remove")) {
                ent.remove_component<light_component>();
            }
        } else {
            // if no light component exists on the passed entity, allow the option to add one
            static light_creation_data creation_data;
            static std::vector<type_name_pair> types = { { light_type::point, "Point light" },
                                                         { light_type::spotlight, "Spotlight" } };
            std::vector<const char*> names;
            for (const auto& pair : types) {
                names.push_back(pair.name.c_str());
            }

            static int32_t current_light_type = 0;
            ImGui::Combo("Light type", &current_light_type, names.data(), names.size());
            light_type current_type = types[current_light_type].type;
            switch (current_type) {
            case light_type::spotlight:
                ImGui::InputFloat3("Direction", &creation_data.spotlight_data.direction.x);
                ImGui::SliderFloat("Inner cutoff", &creation_data.spotlight_data.cutoff_angle, 0.f,
                                   45.f);
                ImGui::SliderFloat("Outer cutoff", &creation_data.spotlight_data.outer_cutoff_angle,
                                   0.f, 45.f);
                break;
            }

            if (ImGui::Button("Create")) {
                ref<light> _light;
                switch (current_type) {
                case light_type::spotlight:
                    _light = ref<spotlight>::create(
                        creation_data.spotlight_data.direction,
                        glm::cos(glm::radians(creation_data.spotlight_data.cutoff_angle)),
                        glm::cos(glm::radians(creation_data.spotlight_data.outer_cutoff_angle)));
                    break;
                case light_type::point:
                    _light = ref<point_light>::create();
                    break;
                }
                ent.add_component<light_component>().data = _light;
            }
        }
    }

    enum class model_loading_error { none, no_path, file_does_not_exist };
    static model_loading_error model_error = model_loading_error::none;
    static ref<material> model_material;

    static void model_editor(entity ent) {
        if (ent.has_component<model_component>()) {
            ref<model> _model = ent.get_component<model_component>().data;

            ref<model_source> source = _model->get_source();
            if (source) {
                if (ImGui::Button("Reload")) {
                    source->reload();
                }
            }

            if (ImGui::Button("Remove")) {
                ent.remove_component<model_component>();
            }
            ImGui::Separator();

            const auto& materials = _model->get_materials();
            static int32_t current_material = 0;
            if (current_material >= materials.size()) {
                current_material = 0;
            }
            std::vector<const char*> material_names;
            for (const auto& _material : materials) {
                const auto& name = _material->get_name();
                material_names.push_back(name.c_str());
            }
            ImGui::Combo("Selected material", &current_material, material_names.data(),
                         material_names.size());
            model_material = materials[current_material];

            float available_width = ImGui::GetContentRegionAvail().x;
            float image_size = available_width / 8;

            ImGui::Text("Albedo map");
            ref<texture> tex = model_material->get_texture("albedo_texture");
            ImGui::Image(tex->get_imgui_id(), ImVec2(image_size, image_size));

            ImGui::Text("Specular map");
            tex = model_material->get_texture("specular_texture");
            ImGui::Image(tex->get_imgui_id(), ImVec2(image_size, image_size));

            ImGui::Text("Normal map");
            tex = model_material->get_texture("normal_map");
            ImGui::Image(tex->get_imgui_id(), ImVec2(image_size, image_size));

            glm::vec3 albedo_color = model_material->get_data<glm::vec3>("albedo_color");
            if (ImGui::ColorEdit3("Albedo color", &albedo_color.x)) {
                model_material->set_data("albedo_color", albedo_color);
            }

            glm::vec3 specular_color = model_material->get_data<glm::vec3>("specular_color");
            if (ImGui::ColorEdit3("Specular color", &specular_color.x)) {
                model_material->set_data("specular_color", specular_color);
            }

            float opacity = model_material->get_data<float>("opacity");
            if (ImGui::SliderFloat("Opacity", &opacity, 0.f, 1.f)) {
                model_material->set_data("opacity", opacity);
            }

            float shininess = model_material->get_data<float>("shininess");
            if (ImGui::SliderFloat("Shininess", &shininess, 0.f, 360.f)) {
                model_material->set_data("shininess", shininess);
            }
        } else {
            static fs::path model_path;
            ImGui::InputPath("Model path", &model_path);
            std::string error_message;
            switch (model_error) {
            case model_loading_error::no_path:
                error_message = "No path was provided!";
                break;
            case model_loading_error::file_does_not_exist:
                error_message = "The specified file does not exist!";
                break;
            }
            if (!error_message.empty()) {
                ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "%s", error_message.c_str());
            }
            if (ImGui::Button("Load")) {
                if (model_path.empty()) {
                    model_error = model_loading_error::no_path;
                } else if (!fs::exists(model_path)) {
                    model_error = model_loading_error::file_does_not_exist;
                } else {
                    auto _model = ref<model>::create(ref<model_source>::create(model_path));
                    ent.add_component<model_component>().data = _model;
                    model_path.clear();
                }
            }
        }
    }

    static void script_editor(entity ent) {
        // verify that there are scripts to toggle
        std::vector<ref<script>> scripts;
        if (ent.has_component<script_component>()) {
            scripts = ent.get_component<script_component>().scripts;
        }
        if (scripts.empty()) {
            ImGui::Text("This entity does not have any scripts bound.");
            return;
        }

        // if there's more than one script, enable the option to toggle all of them
        std::optional<bool> mass_toggle;
        if (scripts.size() > 1) {
            if (ImGui::Button("Toggle all on")) {
                mass_toggle = true;
            }
            if (ImGui::Button("Toggle all off")) {
                mass_toggle = false;
            }
        }

        for (size_t i = 0; i < scripts.size(); i++) {
            ref<script> _script = scripts[i];
            bool call_script = false;

            bool enabled = _script->enabled();
            if (mass_toggle && *mass_toggle != enabled) {
                enabled = *mass_toggle;
                call_script = true;
            }

            std::string label = "Script " + std::to_string(i + 1);
            if (ImGui::Checkbox(label.c_str(), &enabled)) {
                call_script = true;
            }

            if (call_script) {
                if (enabled) {
                    _script->enable();
                } else {
                    _script->disable();
                }
            }
        }
    }

    static void track_editor(entity ent) {
        if (ent.has_component<track_segment_component>()) {
            auto& track_data = ent.get_component<track_segment_component>();

            // next track
            {
                ref<scene> _scene = application::get_scene();
                auto view = _scene->view<tag_component, transform_component, track_segment_component>();

                std::vector<const char*> names = { "N/A" };
                std::vector<entity> entities(1);
                for (entity track : view) {
                    if (track == ent) continue;

                    const auto& tag = track.get_component<tag_component>().tag;
                    names.push_back(tag.c_str());
                    entities.push_back(track);
                }

                int32_t track_index = -1;
                for (size_t i = 0; i < entities.size(); i++) {
                    if (track_data.next == entities[i]) {
                        track_index = (int32_t)i;
                    }
                }

                bool changed_next_node = false;
                if (track_index < 0) {
                    track_index = 0;
                    changed_next_node = true;
                }

                if (ImGui::Combo("Next track", &track_index, names.data(), names.size())) {
                    changed_next_node = true;
                }

                if (changed_next_node) {
                    track_data.next = entities[track_index];
                    _scene->reevaluate_first_track_node();
                }
            }

            if (ImGui::Button("Remove")) {
                ent.remove_component<track_segment_component>();
            }
        } else {
            if (ImGui::Button("Add")) {
                ent.add_component<track_segment_component>();
            }
        }
    }

    inspector::~inspector() {
        if (model_material) {
            model_material.reset();
        }
    }

    void inspector::update() {
        ImGui::Begin("Inspector", &this->m_open);
        ref<scene> _scene = application::get_scene();

        {
            scene_serializer serializer(_scene);

            static fs::path write_path;
            ImGui::InputPath("##write-path", &write_path);
            ImGui::SameLine();
            if (ImGui::Button("Save")) {
                if (write_path.has_parent_path()) {
                    fs::path directory = write_path.parent_path();
                    if (!fs::exists(directory)) {
                        fs::create_directories(directory);
                    }
                }

                serializer.serialize(write_path);
            }
        }

        bool reset_name = false;
        if (ImGui::Button("Add entity")) {
            _scene->create();
            reset_name = true;
        }

        std::vector<entity> entities = _scene->view<transform_component>();
        if (!entities.empty()) {
            static int32_t current_entity = 0;
            if (current_entity >= entities.size()) {
                current_entity = entities.size() - 1;
            }

            std::vector<const char*> names;
            for (entity ent : entities) {
                names.push_back(ent.get_component<tag_component>().tag.c_str());
            }

            bool entity_changed = false;
            static std::string temp_name = names[current_entity];
            ImGui::SameLine();
            if (ImGui::Combo("##entity", &current_entity, names.data(), names.size())) {
                reset_name = true;
                entity_changed = true;
            }
            if (reset_name) {
                temp_name = names[current_entity];
            }
            entity ent = entities[current_entity];

            ImGui::InputText("##edit-tag", &temp_name);
            ImGui::SameLine();
            if (ImGui::Button("Set name")) {
                ent.get_component<tag_component>().tag = temp_name;
            }

            auto& transform = ent.get_component<transform_component>();
            constexpr float speed = 0.05f;
            ImGui::DragFloat3("Translation", &transform.translation.x, speed);
            glm::vec3 degrees = glm::degrees(transform.rotation);
            ImGui::DragFloat3("Rotation", &degrees.x, speed);
            transform.rotation = glm::radians(degrees);
            ImGui::DragFloat3("Scale", &transform.scale.x, speed);

            if (ImGui::CollapsingHeader("Light")) {
                ImGui::Indent();
                light_editor(ent);
                ImGui::Unindent();
            }

            bool reset_model_editor_data = entity_changed;
            if (ImGui::CollapsingHeader("Model")) {
                ImGui::Indent();
                model_editor(ent);
                ImGui::Unindent();
            } else {
                reset_model_editor_data = true;
            }
            if (reset_model_editor_data) {
                if (model_error != model_loading_error::none) {
                    model_error = model_loading_error::none;
                }
                if (model_material) {
                    model_material.reset();
                }
            }

            if (ImGui::CollapsingHeader("Scripts")) {
                ImGui::Indent();
                script_editor(ent);
                ImGui::Unindent();
            }

            if (ImGui::CollapsingHeader("Track")) {
                ImGui::Indent();
                track_editor(ent);
                ImGui::Unindent();
            }
        }

        ImGui::End();
    }
} // namespace vkrollercoaster