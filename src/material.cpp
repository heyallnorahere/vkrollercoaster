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
#include "material.h"
#include "renderer.h"
#include "light.h"
namespace vkrollercoaster {
    static struct {
        ref<swapchain> swap_chain;
    } material_data;
    void material::init(ref<swapchain> swap_chain) {
        if (material_data.swap_chain) {
            spdlog::warn("initializing the material system more than once is not recommended");
        }
        material_data.swap_chain = swap_chain;
    }
    void material::shutdown() {
        if (!material_data.swap_chain) {
            spdlog::warn("the material system has not been initialized");
        }
        material_data.swap_chain.reset();
    }
    material::material(ref<shader> _shader) {
        this->m_swapchain = material_data.swap_chain;
        if (!this->m_swapchain) {
            throw std::runtime_error("the material system has not been initialized!");
        }
        this->m_shader = _shader;
        if (!this->m_shader) {
            throw std::runtime_error("passed nullptr!");
        }

        // default name
        this->m_name = "Material";

        auto& reflection_data = this->m_shader->get_reflection_data();
        if (!reflection_data.find_resource("material_data", this->m_set, this->m_binding)) {
            throw std::runtime_error("could not find material buffer!");
        }
        this->m_buffer = uniform_buffer::from_shader_data(this->m_shader, this->m_set, this->m_binding);
        
        std::string shader_name;
        std::vector<std::string> shader_names;
        shader_library::get_names(shader_names);
        for (const auto& name : shader_names) {
            if (shader_library::get(name) == this->m_shader) {
                shader_name = name;
            }
        }
        if (shader_name.empty()) {
            // the passed shader was not in the library - vkrollercoaster::light has not created a buffer for it
            uint32_t set, binding;
            if (!reflection_data.find_resource("light_data", set, binding)) {
                throw std::runtime_error("the passed shader does not have a light buffer!");
            }
            this->m_light_buffer = uniform_buffer::from_shader_data(this->m_shader, set, binding);
        } else {
            this->m_light_buffer = light::get_buffer(shader_name);
            if (!this->m_light_buffer) {
                throw std::runtime_error("the passed shader does not have a light buffer!");
            }
        }

        for (const auto& [set, resources] : reflection_data.resources) {
            for (const auto& [binding, resource] : resources) {
                if (resource.resource_type == shader_resource_type::sampledimage) {
                    auto white_texture = renderer::get_white_texture();
                    const auto& type = reflection_data.types[resource.type];
                    this->m_textures[resource.name].resize(type.array_size);
                    for (uint32_t i = 0; i < type.array_size; i++) {
                        this->m_textures[resource.name][i] = white_texture;
                    }
                }
            }
        }
    }
    material::~material() {
        for (pipeline* _pipeline : this->m_created_pipelines) {
            _pipeline->m_material = nullptr;
        }
    }
    ref<pipeline> material::create_pipeline(const pipeline_spec& spec) {
        auto _pipeline = ref<pipeline>::create(this->m_swapchain, this->m_shader, spec);
        renderer::get_camera_buffer()->bind(_pipeline);
        this->m_light_buffer->bind(_pipeline);
        this->m_buffer->bind(_pipeline);
        for (const auto& [resource_name, textures] : this->m_textures) {
            for (size_t slot = 0; slot < textures.size(); slot++) {
                textures[slot]->bind(_pipeline, resource_name, slot);
            }
        }
        _pipeline->m_material = this;
        this->m_created_pipelines.insert(_pipeline.raw());
        return _pipeline;
    }
    void material::set_texture(const std::string& name, ref<texture> tex, uint32_t slot) {
        if (this->m_textures.find(name) == this->m_textures.end()) {
            throw std::runtime_error("the specified texture resource does not exist!");
        }
        this->m_textures[name][slot] = tex;
        for (pipeline* _pipeline : this->m_created_pipelines) {
            tex->bind(_pipeline, name, slot);
        }
    }
    ref<texture> material::get_texture(const std::string& name, uint32_t slot) {
        if (this->m_textures.find(name) == this->m_textures.end()) {
            throw std::runtime_error("the specified texture resource does not exist!");
        }
        return this->m_textures[name][slot];
    }
}