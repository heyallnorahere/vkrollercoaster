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
#include "application.h"
#include "renderer.h"
#include "swapchain.h"
#include "shader.h"
#include "pipeline.h"
#include "command_buffer.h"
#include "buffers.h"
#include "util.h"
#include "texture.h"
#include "model.h"
#include "components.h"
#include "imgui_controller.h"
#include "material.h"
#include "light.h"
namespace vkrollercoaster {
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
        bool running = false;
        bool should_stop = false;
    };
    static std::unique_ptr<app_data_t> app_data;

    static void new_frame() {
        window::poll();
        renderer::new_frame();
        light::reset_buffers();
        app_data->imgui->new_frame();
    }

    static void attenuation_editor(attenuation_settings& attenuation) {
        // based off of https://wiki.ogre3d.org/Light+Attenuation+Shortcut
        if (ImGui::CollapsingHeader("Attenuation")) {
            ImGui::Indent();
            ImGui::InputFloat("Target distance", &attenuation.target_distance);

            // constant
            ImGui::Checkbox("##edit-constant", &attenuation.constant.edit);
            ImGui::SameLine();
            ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;
            if (!attenuation.constant.edit) {
                flags |= ImGuiInputTextFlags_ReadOnly;
                attenuation.constant.value = 1.f;
            }
            ImGui::InputFloat("Constant", &attenuation.constant.value, 0.f, 0.f, "%.3f", flags);

            // linear
            ImGui::Checkbox("##edit-linear", &attenuation.linear.edit);
            ImGui::SameLine();
            flags = ImGuiInputTextFlags_None;
            if (!attenuation.linear.edit) {
                flags |= ImGuiInputTextFlags_ReadOnly;            
                attenuation.linear.value = 4.5f / attenuation.target_distance;
            }
            ImGui::InputFloat("Linear", &attenuation.linear.value, 0.f, 0.f, "%.3f", flags);

            // quadratic
            ImGui::Checkbox("##edit-quadratic", &attenuation.quadratic.edit);
            ImGui::SameLine();
            flags = ImGuiInputTextFlags_None;
            if (!attenuation.quadratic.edit) {
                flags |= ImGuiInputTextFlags_ReadOnly;            
                attenuation.quadratic.value = 75.f / glm::pow(attenuation.target_distance, 2.f);
            }
            ImGui::InputFloat("Quadratic", &attenuation.quadratic.value, 0.f, 0.f, "%.3f", flags);

            ImGui::Unindent();
        }
    }
    struct type_name_pair {
        light_type type;
        std::string name;
    };
    struct spotlight_creation_data {
        glm::vec3 direction = glm::vec3(0.f, -1.f, 0.f);
        float cutoff_angle = 12.5f;
        float outer_cutoff_angle = 17.5f;
    };
    struct light_creation_data {
        // this is it for now
        spotlight_creation_data spotlight_data;
    };
    static void light_editor(entity ent) {
        if (ent.has_component<light_component>()) {
            // if a light component exists on the passed entity, show fields for editing
            ref<light> _light = ent.get_component<light_component>().data;

            ImGui::ColorEdit3("Diffuse color", &_light->diffuse_color().x);
            ImGui::ColorEdit3("Ambient color", &_light->ambient_color().x);
            ImGui::ColorEdit3("Specular color", &_light->specular_color().x);

            switch (_light->get_type()) {
            case light_type::point:
                {
                    ref<point_light> _point_light = _light.as<point_light>();
                    attenuation_editor(_point_light->attenuation());
                }
                break;
            case light_type::spotlight:
                {
                    ref<spotlight> _spotlight = _light.as<spotlight>();
                    ImGui::InputFloat3("Direction", &_spotlight->direction().x);

                    float angle = glm::degrees(glm::acos(_spotlight->cutoff()));
                    ImGui::SliderFloat("Inner cutoff", &angle, 0.f, 45.f);
                    _spotlight->cutoff() = glm::cos(glm::radians(angle));

                    angle = glm::degrees(glm::acos(_spotlight->outer_cutoff()));
                    ImGui::SliderFloat("Outer cutoff", &angle, 0.f, 45.f);
                    _spotlight->outer_cutoff() = glm::cos(glm::radians(angle));

                    attenuation_editor(_spotlight->attenuation());
                }
                break;
            }
            if (ImGui::Button("Remove light")) {
                ent.remove_component<light_component>();
            }
        } else {
            // if no light component exists on the passed entity, allow the option to add one
            static light_creation_data creation_data;
            static std::vector<type_name_pair> types = {
                { light_type::point, "Point light" },
                { light_type::spotlight, "Spotlight" }
            };
            std::vector<const char*> names;
            for (const auto& pair : types) {
                names.push_back(pair.name.c_str());
            }

            static int32_t current_light_type = 0;
            ImGui::Combo("Light type", &current_light_type, names.data(), names.size());
            light_type current_type = types[current_light_type].type;
            switch (current_type) {
            case light_type::spotlight:
                ImGui::InputFloat3("Direction", &creation_data.spotlight_data.direction.x);
                ImGui::SliderFloat("Inner cutoff", &creation_data.spotlight_data.cutoff_angle, 0.f, 45.f);
                ImGui::SliderFloat("Outer cutoff", &creation_data.spotlight_data.outer_cutoff_angle, 0.f, 45.f);
                break;
            }

            if (ImGui::Button("Create")) {
                ref<light> _light;
                switch (current_type) {
                case light_type::spotlight:
                    _light = ref<spotlight>::create(creation_data.spotlight_data.direction,
                                                    glm::cos(glm::radians(creation_data.spotlight_data.cutoff_angle)),
                                                    glm::cos(glm::radians(creation_data.spotlight_data.outer_cutoff_angle)));
                    break;
                case light_type::point:
                    _light = ref<point_light>::create();
                    break;
                }
                ent.add_component<light_component>().data = _light;
            }
        }
    }
    static void update() {
        double time = util::get_time<double>();
        static double last_frame = time;
        double delta_time = time - last_frame;
        last_frame = time;

        app_data->global_scene->update();

        {
            ImGui::Begin("Settings");
            ImGuiIO& io = ImGui::GetIO();
            ImGui::Text("FPS: %f", io.Framerate);
            ImGui::Separator();

            ImGui::Text("Device info");
            VkPhysicalDevice physical_device = renderer::get_physical_device();
            VkPhysicalDeviceProperties device_properties;
            vkGetPhysicalDeviceProperties(physical_device, &device_properties);
            ImGui::Text("Selected device: %s", device_properties.deviceName);
            ImGui::Indent();

            uint32_t major, minor, patch;
            renderer::expand_vulkan_version(device_properties.apiVersion, major, minor, patch);

            std::string vendor, device_type;
            switch (device_properties.vendorID) {
            case 0x1002: vendor = "AMD"; break;
            case 0x10DE: vendor = "NVIDIA"; break;
            case 0x8086: vendor = "Intel"; break;
            case 0x13B5: vendor = "ARM"; break;
            default: vendor = "unknown"; break;
            }

            switch (device_properties.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_CPU: device_type = "CPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: device_type = "discrete GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: device_type = "integrated GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: device_type = "virtual GPU"; break;
            default: device_type = "unknown/other"; break;
            }

            ImGui::Text("Latest available Vulkan version: %u.%u.%u", major, minor, patch);
            ImGui::Text("Vendor: %s", vendor.c_str());
            ImGui::Text("Type: %s", device_type.c_str());

            ImGui::Unindent();
            ImGui::Separator();

            std::vector<const char*> names;
            for (const auto& loaded_model : app_data->loaded_models) {
                names.push_back(loaded_model.name.c_str());
            }
            static int32_t current_model = 0;
            if (ImGui::Combo("Model", &current_model, names.data(), names.size())) {
                for (entity object : app_data->global_scene->find_tag("created-object")) {
                    auto& model_data = object.get_component<model_component>();
                    model_data.data = app_data->loaded_models[current_model].data;
                }
            }
            ImGui::Separator();

            bool reset_name = false;
            if (ImGui::Button("Add entity")) {
                app_data->global_scene->create();
                reset_name = true;
            }
            std::vector<entity> entities = app_data->global_scene->view<transform_component>();
            if (!entities.empty()) {
                static int32_t current_entity = 0;
                if (current_entity >= entities.size()) {
                    current_entity = entities.size() - 1;
                }

                std::vector<const char*> names;
                for (entity ent : entities) {
                    names.push_back(ent.get_component<tag_component>().tag.c_str());
                }

                static std::string temp_name = names[current_entity];
                ImGui::SameLine();
                if (ImGui::Combo("##entity", &current_entity, names.data(), names.size())) {
                    reset_name = true;
                }
                if (reset_name) {
                    temp_name = names[current_entity];
                }
                entity ent = entities[current_entity];

                ImGui::InputText("##edit-tag", &temp_name);
                ImGui::SameLine();
                if (ImGui::Button("Set name")) {
                    ent.get_component<tag_component>().tag = temp_name;
                }

                auto& transform = ent.get_component<transform_component>();
                constexpr float speed = 0.05f;
                ImGui::DragFloat3("Translation", &transform.translation.x, speed);
                glm::vec3 degrees = glm::degrees(transform.rotation);
                ImGui::DragFloat3("Rotation", &degrees.x, speed);
                transform.rotation = glm::radians(degrees);
                ImGui::DragFloat3("Scale", &transform.scale.x, speed);

                if (ImGui::CollapsingHeader("Light")) {
                    ImGui::Indent();
                    light_editor(ent);
                    ImGui::Unindent();
                }
            }
            ImGui::Separator();

            if (ImGui::Button("Reload shaders")) {
                std::vector<std::string> names;
                shader_library::get_names(names);
                for (const auto& name : names) {
                    shader_library::get(name)->reload();
                }
            }

            ImGui::End();
        }

        renderer::update_camera_buffer(app_data->global_scene);
    }

    static void draw(ref<command_buffer> cmdbuffer, size_t current_image) {
        cmdbuffer->begin();
        cmdbuffer->begin_render_pass(app_data->swap_chain, glm::vec4(glm::vec3(0.1f), 1.f), current_image);
        // probably should optimize and batch render
        for (entity ent : app_data->global_scene->view<transform_component, model_component>()) {
            renderer::render(cmdbuffer, ent);
        }
        app_data->imgui->render(cmdbuffer);
        cmdbuffer->end_render_pass();
        cmdbuffer->end();
    }

    void application::init() {
        app_data = std::make_unique<app_data_t>();

        // create window
        window::init();
        app_data->app_window = ref<window>::create(1600, 900, "vkrollercoaster");

        // set up vulkan
        renderer::add_device_extension(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
        renderer::init(app_data->app_window);
        app_data->swap_chain = ref<swapchain>::create();
        app_data->imgui = ref<imgui_controller>::create(app_data->swap_chain);
        material::init(app_data->swap_chain);

        // load shader
        shader_library::add("default_static");

        // create light uniform buffers
        light::init();

        // load app data
        size_t image_count = app_data->swap_chain->get_swapchain_images().size();
        for (const auto& entry : fs::directory_iterator("assets/models")) {
            fs::path path = entry.path();
            if (path.extension() != ".gltf") {
                continue;
            }
            loaded_model_t loaded_model;
            loaded_model.data = ref<model>::create(path);
            loaded_model.name = path.filename().string();
            app_data->loaded_models.push_back(loaded_model);
        }
        for (size_t i = 0; i < image_count; i++) {
            auto cmdbuffer = renderer::create_render_command_buffer();
            app_data->command_buffers.push_back(cmdbuffer);
        }
        app_data->global_scene = ref<scene>::create();
        entity object = app_data->global_scene->create("created-object");
        object.get_component<transform_component>().scale = glm::vec3(0.25f);
        object.add_component<model_component>().data = app_data->loaded_models[0].data;
        {
            entity player = app_data->global_scene->create("player");
            auto& transform = player.get_component<transform_component>();
            transform.translation = glm::vec3(0.f, 0.f, -2.5f);
            player.add_component<camera_component>().primary = true;
        }
    }

    void application::shutdown() {
        // shut down subsystems
        light::shutdown();
        material::shutdown();
        shader_library::clear();
        renderer::shutdown();
        window::shutdown();

        // delete app data
        app_data.reset();
    }

    void application::run() {
        if (app_data->running) {
            throw std::runtime_error("the application is already running!");
        }
        app_data->running = true;

        // game loop
        app_data->should_stop = false;
        while (!app_data->should_stop) {
            // signal a new frame
            new_frame();

            // update app
            update();

            // acquire a new swapchain image
            app_data->swap_chain->prepare_frame();

            // add draw commands
            size_t current_image = app_data->swap_chain->get_current_image();
            auto cmdbuffer = app_data->command_buffers[current_image];
            draw(cmdbuffer, current_image);

            // render and present
            cmdbuffer->submit();
            cmdbuffer->reset();
            app_data->swap_chain->present();
            
            // check to see if window has closed
            if (app_data->app_window->should_close()) {
                app_data->should_stop = true;
            }
        }

        app_data->running = false;
    }
    void application::quit() {
        if (!app_data->running) {
            throw std::runtime_error("the application is not currently running!");
        }

        app_data->should_stop = true;
    }

    ref<window> application::get_window() { return app_data->app_window; }
    ref<scene> application::get_scene() { return app_data->global_scene; }
}