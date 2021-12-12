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
#include "shader.h"
#include "command_buffer.h"
#include "components.h"
#include "imgui_controller.h"
#include "light.h"
#include "menus/menus.h"
#include "input_manager.h"
namespace vkrollercoaster {
    struct app_data_t {
        ref<window> app_window;
        ref<swapchain> swap_chain;
        std::vector<ref<command_buffer>> command_buffers;
        ref<scene> global_scene;
        bool running = false;
        bool should_stop = false;
    };
    static std::unique_ptr<app_data_t> app_data;

    // temporary player script
    class player_behavior : public script {
    public:
        player_behavior() {
            this->m_input_manager = ref<input_manager>::create(app_data->app_window);
        }
        virtual void update() override {
            this->m_input_manager->update();

            double current_time = window::get_time();
            static double last_frame = current_time;
            float delta_time = static_cast<float>(current_time - last_frame);
            last_frame = current_time;

            auto& transform = this->get_component<transform_component>();
            glm::vec2 mouse_offset = this->m_input_manager->get_mouse_offset();

            // camera direction
            transform.rotation += glm::vec3(mouse_offset.y, mouse_offset.x, 0.f) * 0.001f;

            // clamp pitch to -89 to 89
            if (transform.rotation.x > 89.f) {
                transform.rotation.x = 89.f;
            }
            if (transform.rotation.x < -89.f) {
                transform.rotation.x = -89.f;
            }

            float speed = 2.5f * delta_time;
            glm::vec3 movement_direction = glm::toMat4(glm::quat(transform.rotation)) * glm::vec4(0.f, 0.f, 1.f, 1.f);
            movement_direction = glm::normalize(movement_direction);

            const auto& camera = this->get_component<camera_component>();
            glm::vec3 forward = movement_direction * speed;
            glm::vec3 right = glm::normalize(glm::cross(movement_direction, camera.up)) * speed;
            glm::vec3 up = glm::normalize(camera.up) * speed;

            if (this->m_input_manager->get_key(GLFW_KEY_W).held) {
                transform.translation += forward;
            }
            if (this->m_input_manager->get_key(GLFW_KEY_S).held) {
                transform.translation -= forward;
            }

            if (this->m_input_manager->get_key(GLFW_KEY_D).held) {
                transform.translation += right;
            }
            if (this->m_input_manager->get_key(GLFW_KEY_A).held) {
                transform.translation -= right;
            }

            if (this->m_input_manager->get_key(GLFW_KEY_SPACE).held) {
                transform.translation += up;
            }
            if (this->m_input_manager->get_key(GLFW_KEY_LEFT_SHIFT).held) {
                transform.translation -= up;
            }
        }
    private:
        virtual void on_enable() override {
            this->m_input_manager->disable_cursor();
        }
        virtual void on_disable() override {
            this->m_input_manager->enable_cursor();
        }
        ref<input_manager> m_input_manager;
    };

    static void new_frame() {
        window::poll();
        renderer::new_frame();
        light::reset_buffers();
        imgui_controller::new_frame();
    }

    static void update() {
        imgui_controller::update_menus();
        app_data->global_scene->update();
        renderer::update_camera_buffer(app_data->global_scene);
    }

    static void draw(ref<command_buffer> cmdbuffer) {
        cmdbuffer->begin();

        // render to the viewport's framebuffer
        ref<framebuffer> render_framebuffer = viewport::get_instance()->get_framebuffer();
        cmdbuffer->begin_render_pass(render_framebuffer, glm::vec4(glm::vec3(0.1f), 1.f));

        // probably should optimize and batch render
        for (entity ent : app_data->global_scene->view<transform_component, model_component>()) {
            renderer::render(cmdbuffer, ent);
        }

        cmdbuffer->end_render_pass();
        cmdbuffer->begin_render_pass(app_data->swap_chain, glm::vec4(glm::vec3(0.f), 1.f));

        imgui_controller::render(cmdbuffer);

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
        imgui_controller::init(app_data->swap_chain);

        // load shader
        shader_library::add("default_static");

        // create light uniform buffers
        light::init();

        // create command buffers
        size_t image_count = app_data->swap_chain->get_swapchain_images().size();
        for (size_t i = 0; i < image_count; i++) {
            auto cmdbuffer = renderer::create_render_command_buffer();
            app_data->command_buffers.push_back(cmdbuffer);
        }

        // create scene and player
        app_data->global_scene = ref<scene>::create();
        {
            entity player = app_data->global_scene->create("player");
            auto& transform = player.get_component<transform_component>();
            transform.translation = glm::vec3(0.f, 0.f, -2.5f);
            player.add_component<camera_component>().primary = true;
            player.add_component<script_component>().bind<player_behavior>();
        }
    }

    void application::shutdown() {
        // shut down subsystems
        light::shutdown();
        imgui_controller::shutdown();
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
            draw(cmdbuffer);

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
    bool application::running() { return app_data->running; }

    ref<window> application::get_window() { return app_data->app_window; }
    ref<scene> application::get_scene() { return app_data->global_scene; }
    ref<swapchain> application::get_swapchain() { return app_data->swap_chain; }
}