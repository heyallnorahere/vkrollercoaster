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
#include "render_target.h"
#include "texture.h"
#include "shader.h"
#include "buffers.h"
#include "pipeline.h"
namespace vkrollercoaster {
    class material : public ref_counted {
    public:
        material(ref<shader> _shader);
        material(const std::string& shader_name) : material(shader_library::get(shader_name)) {}
        ~material();
        ref<pipeline> create_pipeline(ref<render_target> target, const pipeline_spec& spec);
        void set_name(const std::string& name) { this->m_name = name; }
        const std::string& get_name() { return this->m_name; }
        template <typename T> void set_data(const std::string& name, const T& data) {
            auto& reflection_data = this->m_shader->get_reflection_data();
            auto& resource = reflection_data.resources[this->m_set][this->m_binding];
            const auto& resource_type = reflection_data.types[resource.type];
            if (!resource_type.path_exists(name)) {
                throw std::runtime_error("could not find the specified field!");
            }
            size_t offset = resource_type.find_offset(name);
            this->m_buffer->set_data(data, offset);
        }
        template <typename T> T get_data(const std::string& name) {
            auto& reflection_data = this->m_shader->get_reflection_data();
            const auto& resource = reflection_data.resources[this->m_set][this->m_binding];
            const auto& resource_type = reflection_data.types[resource.type];
            if (!resource_type.path_exists(name)) {
                throw std::runtime_error("could not find the specified field!");
            }
            size_t offset = resource_type.find_offset(name);
            T data;
            this->m_buffer->get_data(data, offset);
            return data;
        }
        void set_texture(const std::string& name, ref<texture> tex, uint32_t slot = 0);
        ref<texture> get_texture(const std::string& name, uint32_t slot = 0);

    private:
        ref<uniform_buffer> m_buffer, m_light_buffer;
        ref<shader> m_shader;
        std::string m_name;
        std::map<std::string, std::vector<ref<texture>>> m_textures;
        uint32_t m_set, m_binding;
        std::set<pipeline*> m_created_pipelines;
        friend class pipeline;
    };
} // namespace vkrollercoaster