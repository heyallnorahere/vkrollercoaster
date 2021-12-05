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
            case light_type::point:
                {
                    ref<point_light> _point_light = _light.as<point_light>();
                    attenuation_editor(_point_light->attenuation());
                }
                break;
            case light_type::spotlight:
                {
                    ref<spotlight> _spotlight = _light.as<spotlight>();
                    ImGui::InputFloat3("Direction", &_spotlight->direction().x);

                    float angle = glm::degrees(glm::acos(_spotlight->cutoff()));
                    ImGui::SliderFloat("Inner cutoff", &angle, 0.f, 45.f);
                    _spotlight->cutoff() = glm::cos(glm::radians(angle));

                    angle = glm::degrees(glm::acos(_spotlight->outer_cutoff()));
                    ImGui::SliderFloat("Outer cutoff", &angle, 0.f, 45.f);
                    _spotlight->outer_cutoff() = glm::cos(glm::radians(angle));

                    attenuation_editor(_spotlight->attenuation());
                }
                break;
            }
            if (ImGui::Button("Remove")) {
                ent.remove_component<light_component>();
            }
        } else {
            // if no light component exists on the passed entity, allow the option to add one
            static light_creation_data creation_data;
            static std::vector<type_name_pair> types = {
                { light_type::point, "Point light" },
                { light_type::spotlight, "Spotlight" }
            };
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
                ImGui::SliderFloat("Inner cutoff", &creation_data.spotlight_data.cutoff_angle, 0.f, 45.f);
                ImGui::SliderFloat("Outer cutoff", &creation_data.spotlight_data.outer_cutoff_angle, 0.f, 45.f);
                break;
            }

            if (ImGui::Button("Create")) {
                ref<light> _light;
                switch (current_type) {
                case light_type::spotlight:
                    _light = ref<spotlight>::create(creation_data.spotlight_data.direction,
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
    enum class model_loading_error {
        none,
        no_path,
        file_does_not_exist
    };
    static model_loading_error model_error = model_loading_error::none;
    static ref<material> model_material;
    static void model_editor(entity ent) {
        if (ent.has_component<model_component>()) {
            ref<model> _model = ent.get_component<model_component>().data;

            std::string string_path = _model->get_path().string();
            ImGui::Text("Model: %s", string_path.c_str());
            if (ImGui::Button("Reload")) {
                _model->reload();
            }
            if (ImGui::Button("Remove")) {
                ent.remove_component<model_component>();
            }

            const auto& render_call_data = _model->get_render_call_data();
            static int32_t current_material = 0;
            if (current_material >= render_call_data.size()) {
                current_material = 0;
            }
            std::vector<const char*> material_names;
            for (const auto& render_call : render_call_data) {
                const auto& name = render_call._material._material->get_name();
                material_names.push_back(name.c_str());
            }
            ImGui::Combo("Selected material", &current_material, material_names.data(), material_names.size());
            model_material = render_call_data[current_material]._material._material;

            float available_width = ImGui::GetContentRegionAvail().x;
            float image_size = available_width / 8;

            ImGui::Text("Albedo map");
            ref<texture> albedo_map = model_material->get_texture("albedo_texture");
            ImGui::Image(albedo_map->get_imgui_id(), ImVec2(image_size, image_size));
        } else {
            static std::string model_path;
            ImGui::InputText("Model path", &model_path);
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
                    auto _model = ref<model>::create(model_path);
                    ent.add_component<model_component>().data = _model;
                    model_path.clear();
                }
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

            static std::string temp_name = names[current_entity];
            ImGui::SameLine();
            if (ImGui::Combo("##entity", &current_entity, names.data(), names.size())) {
                reset_name = true;
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

            if (ImGui::CollapsingHeader("Model")) {
                ImGui::Indent();
                model_editor(ent);
                ImGui::Unindent();
            } else {
                if (model_error != model_loading_error::none) {
                    model_error = model_loading_error::none;
                }
                if (model_material) {
                    model_material.reset();
                }
            }
        }

        ImGui::End();
    }
}