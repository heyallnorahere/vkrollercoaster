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
using namespace vkrollercoaster;
struct vertex {
    glm::vec3 position, color;
    glm::vec2 uv;
};
static struct {
    std::shared_ptr<window> app_window;
    std::shared_ptr<swapchain> swap_chain;
    std::shared_ptr<pipeline> test_pipeline;
    std::vector<std::shared_ptr<command_buffer>> command_buffers;
    std::shared_ptr<vertex_buffer> triangle_vertex_buffer;
} app_data;
std::vector<vertex> vertices = {
    { glm::vec3(-0.5f, 0.5f, 0.f), glm::vec3(1.f, 0.f, 0.f), glm::vec2(0.f, 1.f) },
    { glm::vec3(0.f, -0.5f, 0.f), glm::vec3(0.f, 1.f, 0.f), glm::vec2(0.5f, 0.f) },
    { glm::vec3(0.5f, 0.5f, 0.f), glm::vec3(0.f, 0.f, 1.f), glm::vec2(1.f, 1.f) },
};
static void draw(std::shared_ptr<command_buffer> cmdbuffer, size_t current_image) {
    cmdbuffer->begin();
    cmdbuffer->begin_render_pass(app_data.swap_chain, glm::vec4(1.f, 0.f, 1.f, 1.f), current_image);
    app_data.test_pipeline->bind(cmdbuffer, current_image);
    app_data.triangle_vertex_buffer->bind(cmdbuffer);
    vkCmdDraw(cmdbuffer->get(), vertices.size(), 1, 0, 0);
    cmdbuffer->end_render_pass();
    cmdbuffer->end();
}
int32_t main(int32_t argc, const char** argv) {
    window::init();
    app_data.app_window = std::make_shared<window>(1600, 900, "vkrollercoaster");
    renderer::init(app_data.app_window);
    app_data.swap_chain = std::make_shared<swapchain>();
    auto testshader = std::make_shared<shader>("assets/shaders/testshader.glsl");
    vertex_input_data vertex_inputs;
    vertex_inputs.stride = sizeof(vertex);
    vertex_inputs.attributes = {
        { vertex_attribute_type::VEC3, offsetof(vertex, position) },
        { vertex_attribute_type::VEC3, offsetof(vertex, color) },
        { vertex_attribute_type::VEC2, offsetof(vertex, uv) }
    };
    app_data.test_pipeline = std::make_shared<pipeline>(app_data.swap_chain, testshader, vertex_inputs);
    app_data.triangle_vertex_buffer = std::make_shared<vertex_buffer>(vertices);
    size_t image_count = app_data.swap_chain->get_swapchain_images().size();
    for (size_t i = 0; i < image_count; i++) {
        auto cmdbuffer = renderer::create_render_command_buffer();
        app_data.command_buffers.push_back(cmdbuffer);
    }
    while (!app_data.app_window->should_close()) {
        renderer::new_frame();
        app_data.swap_chain->prepare_frame();
        size_t current_image = app_data.swap_chain->get_current_image();
        auto cmdbuffer = app_data.command_buffers[current_image];
        draw(cmdbuffer, current_image);
        cmdbuffer->submit();
        cmdbuffer->reset();
        app_data.swap_chain->present();
        window::poll();
    }
    renderer::shutdown();
    window::shutdown();
    return 0;
}