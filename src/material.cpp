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
namespace vkrollercoaster {
    static struct {
        std::shared_ptr<swapchain> swap_chain;
    } material_data;
    void material::init(std::shared_ptr<swapchain> swap_chain) {
        if (material_data.swap_chain) {
            spdlog::warn("initializing the material system more than once is not recommended");
        }
        material_data.swap_chain = swap_chain;
    }
    void material::shutdown() {
        if (material_data.swap_chain) {
            spdlog::warn("the material system has not been initialized");
        }
        material_data.swap_chain.reset();
    }
    material::material(std::shared_ptr<shader> _shader) {
        this->m_swapchain = material_data.swap_chain;
        if (!this->m_swapchain) {
            throw std::runtime_error("the material system has not been initialized!");
        }
        this->m_shader = _shader;
        auto& reflection_data = this->m_shader->get_reflection_data();
        if (!reflection_data.find_resource("material_data", this->m_set, this->m_binding)) {
            throw std::runtime_error("could not find material buffer!");
        }
        this->m_buffer = uniform_buffer::from_shader_data(this->m_shader, this->m_set, this->m_binding);
    }
    std::shared_ptr<pipeline> material::create_pipeline(const pipeline_spec& spec) {
        auto _pipeline = std::make_shared<pipeline>(this->m_swapchain, this->m_shader, spec);
        size_t image_count = this->m_swapchain->get_swapchain_images().size();
        for (size_t i = 0; i < image_count; i++) {
            this->m_buffer->bind(_pipeline, i);
            for (const auto& [resource_name, textures] : this->m_textures) {
                for (size_t slot = 0; slot < textures.size(); slot++) {
                    textures[slot]->bind(_pipeline, i, resource_name, slot);
                }
            }
        }
        return _pipeline;
    }
    void material::set(const std::string& name, int32_t data) {
        auto& reflection_data = this->m_shader->get_reflection_data();
        auto& resource = reflection_data.resources[this->m_set][this->m_binding];
        size_t offset = reflection_data.types[resource.type].find_offset(name, reflection_data);
        this->m_buffer->set_data(data, offset);
    }
    void material::set(const std::string& name, uint32_t data) {
        auto& reflection_data = this->m_shader->get_reflection_data();
        auto& resource = reflection_data.resources[this->m_set][this->m_binding];
        size_t offset = reflection_data.types[resource.type].find_offset(name, reflection_data);
        this->m_buffer->set_data(data, offset);
    }
    void material::set(const std::string& name, float data) {
        auto& reflection_data = this->m_shader->get_reflection_data();
        auto& resource = reflection_data.resources[this->m_set][this->m_binding];
        size_t offset = reflection_data.types[resource.type].find_offset(name, reflection_data);
        this->m_buffer->set_data(data, offset);
    }
    void material::set(const std::string& name, const glm::vec3& data) {
        auto& reflection_data = this->m_shader->get_reflection_data();
        auto& resource = reflection_data.resources[this->m_set][this->m_binding];
        size_t offset = reflection_data.types[resource.type].find_offset(name, reflection_data);
        this->m_buffer->set_data(data, offset);
    }
    void material::set(const std::string& name, const glm::vec4& data) {
        auto& reflection_data = this->m_shader->get_reflection_data();
        auto& resource = reflection_data.resources[this->m_set][this->m_binding];
        size_t offset = reflection_data.types[resource.type].find_offset(name, reflection_data);
        this->m_buffer->set_data(data, offset);
    }
    void material::set(const std::string& name, const glm::mat4& data) {
        auto& reflection_data = this->m_shader->get_reflection_data();
        auto& resource = reflection_data.resources[this->m_set][this->m_binding];
        size_t offset = reflection_data.types[resource.type].find_offset(name, reflection_data);
        this->m_buffer->set_data(data, offset);
    }
    void material::set(const std::string& name, std::shared_ptr<texture> tex, uint32_t slot) {
        if (slot >= this->m_textures[name].size()) {
            this->m_textures[name].resize(slot + 1);
        }
        this->m_textures[name][slot] = tex;
    }
}