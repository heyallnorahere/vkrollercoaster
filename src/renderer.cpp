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
#include "util.h"
namespace vkrollercoaster {
    static struct {
        std::vector<const char*> instance_extensions, layer_names;
        VkInstance instance = nullptr;
    } renderer_data;
    static void choose_extensions() {
        uint32_t glfw_extension_count = 0;
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
        if (glfw_extension_count > 0) {
            for (uint32_t i = 0; i < glfw_extension_count; i++) {
                renderer_data.instance_extensions.push_back(glfw_extensions[i]);
            }
        }
    }
    static void create_instance() {
        VkApplicationInfo app_info;
        util::zero(app_info);
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.apiVersion = VK_API_VERSION_1_0;
        app_info.pApplicationName = app_info.pEngineName = "vkrollercoaster";
        app_info.applicationVersion = app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0); // todo: git tags
        VkInstanceCreateInfo create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;
        if (!renderer_data.instance_extensions.empty()) {
            create_info.enabledExtensionCount = renderer_data.instance_extensions.size();
            create_info.ppEnabledExtensionNames = renderer_data.instance_extensions.data();
        }
        if (!renderer_data.layer_names.empty()) {
            create_info.enabledLayerCount = renderer_data.layer_names.size();
            create_info.ppEnabledLayerNames = renderer_data.layer_names.data();
        }
        if (vkCreateInstance(&create_info, nullptr, &renderer_data.instance) != VK_SUCCESS) {
            throw std::runtime_error("could not create vulkan instance!");
        }
        spdlog::info("created vulkan instance");
    }
    void renderer::init() {
        choose_extensions();
        create_instance();
    }
    void renderer::shutdown() {
        vkDestroyInstance(renderer_data.instance, nullptr);
    }
};