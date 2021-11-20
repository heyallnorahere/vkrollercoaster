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
#include "pipeline.h"
#include "renderer.h"
namespace vkrollercoaster {
    pipeline::pipeline(std::shared_ptr<swapchain> _swapchain, std::shared_ptr<shader> _shader, const std::vector<vertex_attribute>& attributes) {
        this->m_swapchain = _swapchain;
        this->m_shader = _shader;
        this->m_vertex_attributes = attributes;
        renderer::add_ref();
        this->create();
        this->m_shader->m_dependents.insert(this);
        this->m_swapchain->m_dependents.insert(this);
    }
    pipeline::~pipeline() {
        this->m_swapchain->m_dependents.erase(this);
        this->m_shader->m_dependents.erase(this);
        this->destroy();
        renderer::remove_ref();
    }
    void pipeline::create() {
        // todo: create
    }
    void pipeline::destroy() {
        // todo: destroy
    }
}