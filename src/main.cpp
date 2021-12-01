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
#include "light.h"
using namespace vkrollercoaster;

struct vertex {
    glm::vec3 position, color;
    glm::vec2 uv;
};

struct loaded_model_t {
    ref<model> data;
    std::string name;
};

struct app_data_t {
    ref<window> app_window;
    ref<swapchain> swap_chain;
    std::vector<ref<command_buffer>> command_buffers;
    ref<imgui_controller> imgui;
    ref<scene> global_scene;
    std::vector<loaded_model_t> loaded_models;
    int32_t current_model = 0;
    int32_t current_entity = 0;
    uint64_t frame_count = 0;
};

static void new_frame(app_data_t& app_data) {
    window::poll();
    renderer::new_frame();
    light::reset_buffers();
    app_data.imgui->new_frame();
}

static void update(app_data_t& app_data) {
    double time = util::get_time<double>();
    static double last_frame = time;
    double delta_time = time - last_frame;
    last_frame = time;
    app_data.frame_count++;

    app_data.global_scene->update();

    {
        ImGui::Begin("Settings");
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("FPS: %f", io.Framerate);

        VkPhysicalDevice physical_device = renderer::get_physical_device();
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(physical_device, &device_properties);
        ImGui::Text("Selected device: %s", device_properties.deviceName);

        std::vector<const char*> names;
        for (const auto& loaded_model : app_data.loaded_models) {
            names.push_back(loaded_model.name.c_str());
        }
        if (ImGui::Combo("Model", &app_data.current_model, names.data(), names.size())) {
            for (entity object : app_data.global_scene->find_tag("created-object")) {
                auto& model_data = object.get_component<model_component>();
                model_data.data = app_data.loaded_models[app_data.current_model].data;
            }
        }

        std::vector<entity> entities = app_data.global_scene->view<transform_component>();
        if (!entities.empty()) {
            if (app_data.current_entity >= entities.size()) {
                app_data.current_entity = entities.size() - 1;
            }
            std::vector<const char*> names;
            for (entity ent : entities) {
                if (ent.has_component<tag_component>()) {
                    names.push_back(ent.get_component<tag_component>().tag.c_str());
                } else {
                    names.push_back("Unnamed entity");
                }
            }
            ImGui::Combo("Entity", &app_data.current_entity, names.data(), names.size());
            auto& transform = entities[app_data.current_entity].get_component<transform_component>();
            constexpr float speed = 0.05f;
            ImGui::DragFloat3("Translation", &transform.translation.x, speed);
            glm::vec3 degrees = glm::degrees(transform.rotation);
            ImGui::DragFloat3("Rotation", &degrees.x, speed);
            transform.rotation = glm::radians(degrees);
            ImGui::DragFloat3("Scale", &transform.scale.x, speed, 0.05f);
        }

        if (ImGui::Button("Reload shaders")) {
            std::vector<std::string> names;
            shader_library::get_names(names);
            for (const auto& name : names) {
                shader_library::get(name)->reload();
            }
        }

        ImGui::End();
    }

    renderer::update_camera_buffer(app_data.global_scene);
}

static void draw(app_data_t& app_data, ref<command_buffer> cmdbuffer, size_t current_image) {
    cmdbuffer->begin();
    cmdbuffer->begin_render_pass(app_data.swap_chain, glm::vec4(glm::vec3(0.1f), 1.f), current_image);
    // probably should optimize and batch render
    for (entity ent : app_data.global_scene->view<transform_component, model_component>()) {
        renderer::render(cmdbuffer, ent);
    }
    app_data.imgui->render(cmdbuffer);
    cmdbuffer->end_render_pass();
    cmdbuffer->end();
}

int32_t main(int32_t argc, const char** argv) {
    app_data_t app_data;

    // create window
    window::init();
    app_data.app_window = ref<window>::create(1600, 900, "vkrollercoaster");

    // set up vulkan
    renderer::init(app_data.app_window);
    app_data.swap_chain = ref<swapchain>::create();
    app_data.imgui = ref<imgui_controller>::create(app_data.swap_chain);
    material::init(app_data.swap_chain);

    // load shader
    shader_library::add("default_static");

    // create light uniform buffers
    light::init();

    // load app data
    size_t image_count = app_data.swap_chain->get_swapchain_images().size();
    for (const auto& entry : fs::directory_iterator("assets/models")) {
        fs::path path = entry.path();
        if (path.extension() != ".gltf") {
            continue;
        }
        loaded_model_t loaded_model;
        loaded_model.data = ref<model>::create(path);
        loaded_model.name = path.filename().string();
        app_data.loaded_models.push_back(loaded_model);
    }
    for (size_t i = 0; i < image_count; i++) {
        auto cmdbuffer = renderer::create_render_command_buffer();
        app_data.command_buffers.push_back(cmdbuffer);
    }
    app_data.global_scene = ref<scene>::create();
    entity object = app_data.global_scene->create("created-object");
    object.get_component<transform_component>().scale = glm::vec3(0.25f);
    object.add_component<model_component>(app_data.loaded_models[app_data.current_model].data);
    {
        entity player = app_data.global_scene->create("player");
        auto& transform = player.get_component<transform_component>();
        transform.translation = glm::vec3(0.f, 0.f, -2.5f);
        player.add_component<camera_component>().primary = true;
    }

    // game loop
    while (!app_data.app_window->should_close()) {
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
    light::shutdown();
    material::shutdown();
    shader_library::clear();
    renderer::shutdown();
    window::shutdown();

    return 0;
}