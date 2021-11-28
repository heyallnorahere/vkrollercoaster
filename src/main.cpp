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
#include "components.h"
#include "imgui_controller.h"
#include "material.h"
using namespace vkrollercoaster;

struct vertex {
    glm::vec3 position, color;
    glm::vec2 uv;
};

struct draw_call_data {
    std::shared_ptr<vertex_buffer> vbo;
    std::shared_ptr<index_buffer> ibo;
};

struct app_data_t {
    std::shared_ptr<window> app_window;
    std::shared_ptr<swapchain> swap_chain;
    std::shared_ptr<pipeline> test_pipeline;
    std::vector<std::shared_ptr<command_buffer>> command_buffers;
    std::vector<draw_call_data> rendered_draw_calls;
    std::shared_ptr<uniform_buffer> camera_buffer;
    std::shared_ptr<imgui_controller> imgui;
    std::shared_ptr<scene> global_scene;
    uint64_t frame_count = 0;
};

static void new_frame(app_data_t& app_data) {
    renderer::new_frame();
    app_data.rendered_draw_calls.clear();
    app_data.imgui->new_frame();
}

static void update(app_data_t& app_data) {
    static double last_frame = 0;
    double time = util::get_time<double>();
    double delta_time = time - last_frame;
    last_frame = time;
    // capping it at 60
    constexpr double fps = 60;
    constexpr double frame_duration = 1 / fps;

    if (frame_duration > delta_time) {
        std::this_thread::sleep_for(std::chrono::duration<double>(frame_duration - delta_time));
    }
    app_data.frame_count++;

    static float distance = 2.5f;
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
    for (entity knight : app_data.global_scene->find_tag("knight")) {
        auto& transform = knight.get_component<transform_component>();
        transform.rotation += glm::radians(glm::vec3(1.f, 0.f, 0.f));
    }

    {
        static bool wireframe = false;
        auto& spec = app_data.test_pipeline->spec();
        bool reload_pipeline = false;
        ImGui::Begin("Settings");
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("FPS: %f", io.Framerate);
        if (ImGui::Checkbox("Wireframe", &wireframe)) {
            spec.polygon_mode = (wireframe ? pipeline_polygon_mode::wireframe : pipeline_polygon_mode::fill);
            reload_pipeline = true;
        }
        ImGui::SliderFloat("Distance from object", &distance, 0.5f, 10.f);
        ImGui::End();
        if (reload_pipeline) {
            app_data.test_pipeline->reload();
        }
    }
}

static void draw(app_data_t& app_data, std::shared_ptr<command_buffer> cmdbuffer, size_t current_image) {
    cmdbuffer->begin();
    cmdbuffer->begin_render_pass(app_data.swap_chain, glm::vec4(glm::vec3(0.1f), 1.f), current_image);
    app_data.test_pipeline->bind(cmdbuffer, current_image);
    // probably should optimize and batch render
    for (entity ent : app_data.global_scene->view<transform_component, model_component>()) {
        const auto& model_data = ent.get_component<model_component>();
        const auto& transform = ent.get_component<transform_component>();
        glm::mat4 model = transform.matrix();
        draw_call_data draw_call;
        draw_call.vbo = std::make_shared<vertex_buffer>(model_data.data->get_vertices());
        draw_call.ibo = std::make_shared<index_buffer>(model_data.data->get_indices());
        draw_call.vbo->bind(cmdbuffer);
        draw_call.ibo->bind(cmdbuffer);
        vkCmdPushConstants(cmdbuffer->get(), app_data.test_pipeline->get_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);
        vkCmdDrawIndexed(cmdbuffer->get(), draw_call.ibo->get_index_count(), 1, 0, 0, 0);
        app_data.rendered_draw_calls.push_back(draw_call);
    }
    app_data.imgui->render(cmdbuffer);
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
    app_data.imgui = std::make_shared<imgui_controller>(app_data.swap_chain);
    material::init(app_data.swap_chain);

    // load app data
    shader_library::add("default_static");
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
    app_data.camera_buffer = uniform_buffer::from_shader_data(testshader, 0, 0);
    size_t image_count = app_data.swap_chain->get_swapchain_images().size();
    for (size_t i = 0; i < image_count; i++) {
        auto cmdbuffer = renderer::create_render_command_buffer();
        app_data.command_buffers.push_back(cmdbuffer);
        app_data.camera_buffer->bind(app_data.test_pipeline, i);
    }
    app_data.global_scene = std::make_shared<scene>();
    auto knight_model = std::make_shared<model>("assets/models/knight.gltf");
    entity knight = app_data.global_scene->create("knight");
    knight.get_component<transform_component>().scale = glm::vec3(0.25f);
    knight.add_component<model_component>(knight_model);

    // game loop
    while (!app_data.app_window->should_close()) {
        window::poll();
        new_frame(app_data);
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
    material::shutdown();
    shader_library::clear();
    renderer::shutdown();
    window::shutdown();

    return 0;
}