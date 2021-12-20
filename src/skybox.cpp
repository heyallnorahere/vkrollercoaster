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
#define EXPOSE_IMAGE_UTILS
#include "skybox.h"
#include "model.h"
#include "menus/menus.h"
namespace vkrollercoaster {
    static struct {
        ref<vertex_buffer> vertices;
        ref<index_buffer> indices;
    } skybox_data;

    void skybox::init() {
        // load the mesh
        auto source = ref<model_source>::create("assets/models/cube.gltf");
        auto _model = ref<model>::create(source);

        // vertices
        const auto& vertices = _model->get_vertices();
        std::vector<glm::vec3> positions;
        for (const auto& _vertex : vertices) {
            positions.push_back(_vertex.position);
        }
        skybox_data.vertices = ref<vertex_buffer>::create(vertices);

        // indices
        const auto& indices = _model->get_indices();
        skybox_data.indices = ref<index_buffer>::create(indices);
    }

    void skybox::shutdown() {
        skybox_data.vertices.reset();
        skybox_data.indices.reset();
    }

    struct skybox_data_t {
        float exposure, gamma;
    };

    skybox::skybox(ref<image_cube> cube_map) {
        auto _shader = shader_library::get("skybox");

        // define pipeline layout
        pipeline_spec spec;
        spec.enable_blending = false;
        spec.enable_depth_testing = true;
        spec.input_layout.stride = sizeof(glm::vec3);
        spec.input_layout.attributes = { { vertex_attribute_type::VEC3, 0 } };

        // create pipeline
        ref<render_target> rendertarget = viewport::get_instance()->get_framebuffer();
        this->m_pipeline = ref<pipeline>::create(rendertarget, _shader, spec);

        // create uniform buffer for skybox.hlsl
        auto& reflection_data = _shader->get_reflection_data();
        uint32_t set, binding;
        if (!reflection_data.find_resource("skybox_data", set, binding)) {
            throw std::runtime_error("could not find the skybox uniform buffer!");
        }
        this->m_uniform_buffer = uniform_buffer::from_shader_data(_shader, set, binding);

        // set initial exposure and gamma
        skybox_data_t initial_data;
        initial_data.exposure = 4.5f;
        initial_data.gamma = 2.2f;
        this->m_uniform_buffer->set_data(initial_data);
    }
} // namespace vkrollercoaster