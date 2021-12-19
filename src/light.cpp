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
#include "light.h"
#include "shader.h"
#include "components.h"
namespace vkrollercoaster {
    struct light_buffer_data {
        ref<uniform_buffer> buffer;
        uint32_t set, binding;
    };
    static struct {
        std::unordered_map<std::string, light_buffer_data> buffers;
        void* callback_identifier = nullptr;
    } light_data;
    static void add_shader_buffer(const std::string& name) {
        auto _shader = shader_library::get(name);
        auto& reflection_data = _shader->get_reflection_data();
        light_buffer_data buffer_data;
        if (!reflection_data.find_resource("light_data", buffer_data.set, buffer_data.binding)) {
            spdlog::warn("shader {0} does not have a light buffer", name);
            return;
        }
        buffer_data.buffer =
            uniform_buffer::from_shader_data(_shader, buffer_data.set, buffer_data.binding);
        light_data.buffers.insert(std::make_pair(name, buffer_data));
    }
    void light::init() {
        std::vector<std::string> shader_names;
        shader_library::get_names(shader_names);
        for (const auto& shader_name : shader_names) {
            add_shader_buffer(shader_name);
        }

        light_data.callback_identifier = &light_data;
        shader_library::callbacks_t callbacks;
        callbacks.on_added = [](const std::string& name) { add_shader_buffer(name); };
        callbacks.on_removed = [](const std::string& name, ref<shader> _shader) {
            light_data.buffers.erase(name);
        };
        shader_library::add_callbacks(light_data.callback_identifier, callbacks);
    }
    void light::shutdown() {
        shader_library::remove_callbacks(light_data.callback_identifier);
        light_data.buffers.clear();
    }
    ref<uniform_buffer> light::get_buffer(const std::string& shader_name) {
        ref<uniform_buffer> buffer;
        if (light_data.buffers.find(shader_name) != light_data.buffers.end()) {
            buffer = light_data.buffers[shader_name].buffer;
        }
        return buffer;
    }
    void light::reset_buffers() {
        for (const auto& [name, buffer] : light_data.buffers) {
            buffer.buffer->zero();
        }
    }
    void light::update_buffers(const std::vector<entity>& entities) {
        std::string light_type_name;
        switch (this->get_type()) {
        case light_type::spotlight:
            light_type_name = "spotlight";
            break;
        case light_type::point:
            light_type_name = "point_light";
            break;
        case light_type::directional:
            light_type_name = "directional_light";
            break;
        default:
            throw std::runtime_error("invalid light type!");
        }
        std::string count_field_name = light_type_name + "_count";
        std::string array_field_name = light_type_name + "s";
        for (const auto& [shader_name, buffer] : light_data.buffers) {
            auto _shader = shader_library::get(shader_name);
            auto& reflection_data = _shader->get_reflection_data();
            const auto& resource = reflection_data.resources[buffer.set][buffer.binding];
            auto& type = reflection_data.types[resource.type];
            if (type.fields.find(count_field_name) == type.fields.end()) {
                throw std::runtime_error("shader " + shader_name + " does not have a " +
                                         count_field_name + " field!");
            }
            if (type.fields.find(array_field_name) == type.fields.end()) {
                throw std::runtime_error("shader " + shader_name + " does not have a " +
                                         array_field_name + " field!");
            }
            auto& array_type = reflection_data.types[type.fields[array_field_name].type];
            if (array_type.array_stride == 0) {
                throw std::runtime_error(array_field_name + " is not an array!");
            }
            size_t count_offset = type.find_offset(count_field_name);
            for (entity ent : entities) {
                // get light index
                int32_t count;
                buffer.buffer->get_data(count, count_offset);
                size_t light_index = count++;
                if (light_index >= array_type.array_size) {
                    throw std::runtime_error("cannot have more than " +
                                             std::to_string(array_type.array_size) +
                                             " of this light type!");
                }
                buffer.buffer->set_data(count, count_offset);

                // set up "set" lambda
                std::string entry_prefix =
                    array_field_name + "[" + std::to_string(light_index) + "].";
                auto buffer_instance = buffer.buffer;
                set_callback_t set = [&](const std::string& field_name, const void* data,
                                         size_t size, bool optional) {
                    if (!array_type.path_exists(field_name)) {
                        if (optional) {
                            return;
                        } else {
                            throw std::runtime_error("field \"" + field_name +
                                                     "\" does not exist!");
                        }
                    }
                    size_t offset = type.find_offset(entry_prefix + field_name);
                    buffer_instance->set_data(data, size, offset);
                };

                // copy data
                glm::vec3 position = ent.get_component<transform_component>().translation;
                set("position", &position, sizeof(glm::vec3), true);
                set("diffuse_color", &this->m_diffuse_color, sizeof(glm::vec3), false);
                set("specular_color", &this->m_specular_color, sizeof(glm::vec3), false);
                set("ambient_color", &this->m_ambient_color, sizeof(glm::vec3), false);

                // set data such as attenuation values for point lights or direction for spotlights
                // and directional lights
                this->update_typed_light_data(set);
            }
        }
    }
    void attenuation_settings::update(light::set_callback_t set) {
        const std::string prefix = "attenuation.";
        set(prefix + "_constant", &this->constant.value, sizeof(float), false);
        set(prefix + "_linear", &this->linear.value, sizeof(float), false);
        set(prefix + "_quadratic", &this->quadratic.value, sizeof(float), false);
    }
    point_light::point_light(const attenuation_settings& attenuation) {
        this->m_attenuation = attenuation;
    }
    void point_light::update_typed_light_data(set_callback_t set) {
        this->m_attenuation.update(set);
    }
    spotlight::spotlight(const glm::vec3& direction, float cutoff, float outer_cutoff,
                         const attenuation_settings& attenuation) {
        this->m_direction = direction;
        this->m_cutoff = cutoff;
        this->m_outer_cutoff = outer_cutoff;
        this->m_attenuation = attenuation;
    }
    void spotlight::update_typed_light_data(set_callback_t set) {
        this->m_attenuation.update(set);
        set("direction", &this->m_direction, sizeof(glm::vec3), false);
        set("cutoff", &this->m_cutoff, sizeof(float), false);
        set("outer_cutoff", &this->m_outer_cutoff, sizeof(float), false);
    }
} // namespace vkrollercoaster