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
        VkDebugUtilsMessengerEXT debug_messenger = nullptr;
    } renderer_data;
    static VKAPI_ATTR VkBool32 VKAPI_CALL validation_layer_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data) {
        std::string message = "validation layer: " + std::string(callback_data->pMessage);
        switch (message_severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            spdlog::warn(message);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            spdlog::error(message);
            throw std::runtime_error("vulkan error encountered");
        default:
            // not important enough to show
            break;
        };
        return VK_FALSE;
    }
#ifdef NDEBUG
    constexpr bool enable_validation_layers = false;
#else
    constexpr bool enable_validation_layers = true;
#endif
    static bool check_layer_availability(const std::vector<const char*>& required_layers) {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
        for (const char* layer_name : required_layers) {
            bool layer_found = false;
            for (const auto& layer : available_layers) {
                if (strcmp(layer.layerName, layer_name) == 0) {
                    layer_found = true;
                    break;
                }
            }
            if (!layer_found) {
                return false;
            }
        }
        return true;
    }
    static void choose_extensions() {
        uint32_t glfw_extension_count = 0;
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
        if (glfw_extension_count > 0) {
            for (uint32_t i = 0; i < glfw_extension_count; i++) {
                renderer_data.instance_extensions.push_back(glfw_extensions[i]);
            }
        }
        if (enable_validation_layers) {
            const std::vector<const char*> validation_layers = {
                "VK_LAYER_KHRONOS_validation"
            };
            if (!check_layer_availability(validation_layers)) {
                throw std::runtime_error("validation layers requested but not supported!");
            }
            renderer_data.instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            renderer_data.layer_names.insert(renderer_data.layer_names.begin(), validation_layers.begin(), validation_layers.end());
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
            throw std::runtime_error("could not create a vulkan instance!");
        }
    }
    static void create_debug_messenger() {
        if (!enable_validation_layers) {
            // in release mode, a debug messenger would only slow down the application
            return;
        }
        VkDebugUtilsMessengerCreateInfoEXT create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        create_info.pfnUserCallback = validation_layer_callback;
        auto fpCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(renderer_data.instance, "vkCreateDebugUtilsMessengerEXT");
        if (fpCreateDebugUtilsMessengerEXT != nullptr) {
            if (fpCreateDebugUtilsMessengerEXT(renderer_data.instance, &create_info, nullptr, &renderer_data.debug_messenger) != VK_SUCCESS) {
                throw std::runtime_error("could not create debug messenger");
            }
        } else {
            spdlog::warn("could not get vkCreateDebugUtilsMessengerEXT function address");
        }
    }
    void renderer::init() {
        choose_extensions();
        create_instance();
        create_debug_messenger();
    }
    void renderer::shutdown() {
        if (renderer_data.debug_messenger != nullptr) {
            auto fpDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(renderer_data.instance, "vkDestroyDebugUtilsMessengerEXT");
            if (fpDestroyDebugUtilsMessengerEXT != nullptr) {
                fpDestroyDebugUtilsMessengerEXT(renderer_data.instance, renderer_data.debug_messenger, nullptr);
            } else {
                spdlog::warn("created debug messenger but could not destroy it - will leak memory");
            }
        }
        vkDestroyInstance(renderer_data.instance, nullptr);
    }
};