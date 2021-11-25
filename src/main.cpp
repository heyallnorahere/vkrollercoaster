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
#include "renderer.h"
#include "window.h"
#include "swapchain.h"
#include "shader.h"
#include "pipeline.h"
#include "command_buffer.h"
#include "buffers.h"
#include "util.h"
#include "texture.h"
#include "model.h"
#include "scene.h"
using namespace vkrollercoaster;

struct vertex {
    glm::vec3 position, color;
    glm::vec2 uv;
};

struct app_data_t {
    std::shared_ptr<window> app_window;
    std::shared_ptr<swapchain> swap_chain;
    std::shared_ptr<pipeline> test_pipeline;
    std::vector<std::shared_ptr<command_buffer>> command_buffers;
    std::shared_ptr<vertex_buffer> knight_vertex_buffer;
    std::shared_ptr<index_buffer> knight_index_buffer;
    std::shared_ptr<uniform_buffer> camera_buffer;
    std::shared_ptr<texture> tux;
    glm::mat4 model;
    std::shared_ptr<scene> global_scene;
};

struct transform_component {
    glm::vec3 position;
};

static void update(app_data_t& app_data) {
    constexpr float distance = 2.5f;
    double time = util::get_time<double>();
    glm::vec3 view_point = glm::vec3(0.f);
    view_point.x = (float)cos(time) * distance;
    view_point.z = (float)sin(time) * distance;
    int32_t width, height;
    app_data.app_window->get_size(&width, &height);
    float aspect_ratio = (float)width / (float)height;
    struct {
        glm::mat4 projection, view;
    } camera_data;
    camera_data.projection = glm::perspective(glm::radians(45.f), aspect_ratio, 0.1f, 100.f);
    camera_data.view = glm::lookAt(view_point, glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
    app_data.camera_buffer->set_data(camera_data);
    float scale = (float)sin(time) * 0.5f + 0.5f;
    app_data.model = glm::scale(glm::mat4(1.f), glm::vec3(scale));
}

static void draw(app_data_t& app_data, std::shared_ptr<command_buffer> cmdbuffer, size_t current_image) {
    cmdbuffer->begin();
    cmdbuffer->begin_render_pass(app_data.swap_chain, glm::vec4(0.f, 0.f, 0.f, 1.f), current_image);
    app_data.test_pipeline->bind(cmdbuffer, current_image);
    app_data.knight_vertex_buffer->bind(cmdbuffer);
    app_data.knight_index_buffer->bind(cmdbuffer);
    vkCmdPushConstants(cmdbuffer->get(), app_data.test_pipeline->get_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &app_data.model);
    vkCmdDrawIndexed(cmdbuffer->get(), app_data.knight_index_buffer->get_index_count(), 1, 0, 0, 0);
    cmdbuffer->end_render_pass();
    cmdbuffer->end();
}

int32_t main(int32_t argc, const char** argv) {
    app_data_t app_data;

    // create window
    window::init();
    app_data.app_window = std::make_shared<window>(1600, 900, "vkrollercoaster");

    // set up vulkan
    renderer::init(app_data.app_window);
    app_data.swap_chain = std::make_shared<swapchain>();

    // load app data
    auto testshader = std::make_shared<shader>("assets/shaders/testshader.glsl");
    pipeline_spec test_pipeline_spec;
    test_pipeline_spec.front_face = pipeline_front_face::clockwise;
    test_pipeline_spec.input_layout.stride = sizeof(model::vertex);
    test_pipeline_spec.input_layout.attributes = {
        { vertex_attribute_type::VEC3, offsetof(model::vertex, position) },
        { vertex_attribute_type::VEC3, offsetof(model::vertex, normal) },
        { vertex_attribute_type::VEC2, offsetof(model::vertex, uv) }
    };
    app_data.test_pipeline = std::make_shared<pipeline>(app_data.swap_chain, testshader, test_pipeline_spec);
    auto knight = std::make_shared<model>("assets/models/knight.gltf");
    app_data.knight_vertex_buffer = std::make_shared<vertex_buffer>(knight->get_vertices());
    app_data.knight_index_buffer = std::make_shared<index_buffer>(knight->get_indices());
    app_data.camera_buffer = uniform_buffer::from_shader_data(testshader, 0, 0);
    app_data.tux = std::make_shared<texture>(image::from_file("assets/tux.png"));
    size_t image_count = app_data.swap_chain->get_swapchain_images().size();
    for (size_t i = 0; i < image_count; i++) {
        auto cmdbuffer = renderer::create_render_command_buffer();
        app_data.command_buffers.push_back(cmdbuffer);
        app_data.camera_buffer->bind(app_data.test_pipeline, i);
        app_data.tux->bind(app_data.test_pipeline, i, "tux");
    }
    app_data.global_scene = std::make_shared<scene>();

    // game loop
    while (!app_data.app_window->should_close()) {
        window::poll();
        renderer::new_frame();
        update(app_data);
        app_data.swap_chain->prepare_frame();
        size_t current_image = app_data.swap_chain->get_current_image();
        auto cmdbuffer = app_data.command_buffers[current_image];
        draw(app_data, cmdbuffer, current_image);
        cmdbuffer->submit();
        cmdbuffer->reset();
        app_data.swap_chain->present();
    }

    // clean up
    renderer::shutdown();
    window::shutdown();

    return 0;
}