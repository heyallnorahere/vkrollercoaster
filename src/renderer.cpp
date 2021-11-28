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
#define EXPOSE_RENDERER_INTERNALS
#include "renderer.h"
#include "util.h"
#include "components.h"
namespace vkrollercoaster {
    static struct {
        std::shared_ptr<window> application_window;
        std::set<std::string> instance_extensions, device_extensions, layer_names;
        VkInstance instance = nullptr;
        VkDebugUtilsMessengerEXT debug_messenger = nullptr;
        VkSurfaceKHR window_surface = nullptr;
        VkPhysicalDevice physical_device = nullptr;
        VkDevice device = nullptr;
        VkQueue graphics_queue = nullptr;
        VkQueue present_queue = nullptr;
        VkDescriptorPool descriptor_pool = nullptr;
        VkCommandPool graphics_command_pool = nullptr;
        std::array<sync_objects, renderer::max_frame_count> frame_sync_objects;
        std::shared_ptr<texture> white_texture;
        size_t current_frame = 0;
        uint32_t ref_count = 0;
        bool should_shutdown = false;
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
    template<typename T> static void parse_vulkan_version(T version, T& major, T& minor, T& patch) {
        // bit twiddling bullshit
        constexpr T major_offset = 22;
        constexpr T minor_offset = 12;
        major = version >> major_offset;
        minor = (version >> minor_offset) & util::create_mask<T>(major_offset - minor_offset);
        patch = version & util::create_mask<T>(minor_offset);
    }
    static bool check_layer_availability(const std::string& layer_name) {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
        bool layer_found = false;
        for (const auto& layer : available_layers) {
            if (layer_name == layer.layerName) {
                layer_found = true;
                break;
            }
        }
        if (!layer_found) {
            return false;
        }
        return true;
    }
    void renderer::add_layer(const std::string& name) {
        if (renderer_data.layer_names.find(name) != renderer_data.layer_names.end()) {
            return;
        }
        if (!check_layer_availability(name)) {
            throw std::runtime_error("attempted to add unsupported layer!");
        }
        renderer_data.layer_names.insert(name);
    }
    void renderer::add_instance_extension(const std::string& name) {
        if (renderer_data.instance_extensions.find(name) != renderer_data.instance_extensions.end()) {
            return;
        }
        uint32_t extension_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> extensions(extension_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());
        bool found = false;
        for (const auto& extension : extensions) {
            if (extension.extensionName == name) {
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::runtime_error("the requested instance extension is not available!");
        }
        renderer_data.instance_extensions.insert(name);
    }
    void renderer::add_device_extension(const std::string& name) {
        // cant do any checks without a physical device
        renderer_data.device_extensions.insert(name);
    }
    static void choose_device_extensions() {
        const std::vector<std::string> required_extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
        for (const auto& extension : required_extensions) {
            renderer::add_device_extension(extension);
        }
    }
    static void choose_extensions() {
        choose_device_extensions();
        uint32_t glfw_extension_count = 0;
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
        if (glfw_extension_count > 0) {
            for (uint32_t i = 0; i < glfw_extension_count; i++) {
                renderer::add_instance_extension(glfw_extensions[i]);
            }
        }
        if (enable_validation_layers) {
            const std::vector<std::string> validation_layers = {
                "VK_LAYER_KHRONOS_validation"
            };
            for (const auto& layer_name : validation_layers) {
                renderer::add_layer(layer_name);
            }
            renderer::add_instance_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
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
        std::vector<const char*> extensions, layer_names;
        for (const auto& extension : renderer_data.instance_extensions) {
            extensions.push_back(extension.c_str());
        }
        for (const auto& layer_name : renderer_data.layer_names) {
            layer_names.push_back(layer_name.c_str());
        }
        if (!extensions.empty()) {
            create_info.enabledExtensionCount = extensions.size();
            create_info.ppEnabledExtensionNames = extensions.data();
        }
        if (!layer_names.empty()) {
            create_info.enabledLayerCount = layer_names.size();
            create_info.ppEnabledLayerNames = layer_names.data();
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
    static void create_window_surface() {
        GLFWwindow* glfw_window = renderer_data.application_window->get();
        if (glfwCreateWindowSurface(renderer_data.instance, glfw_window, nullptr, &renderer_data.window_surface) != VK_SUCCESS) {
            throw std::runtime_error("could not create window surface!");
        }
    }
    static bool check_device_extension_support(VkPhysicalDevice device) {
        uint32_t extension_count = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, extensions.data());
        std::set<std::string> remaining_extensions(renderer_data.device_extensions);
        for (const auto& extension : extensions) {
            remaining_extensions.erase(extension.extensionName);
        }
        return remaining_extensions.empty();
    }
    static bool is_device_suitable(VkPhysicalDevice device) {
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceProperties(device, &properties);
        vkGetPhysicalDeviceFeatures(device, &features);
        bool extensions_supported = check_device_extension_support(device);
        bool swapchain_adequate = false;
        if (extensions_supported) {
            auto details = renderer::query_swapchain_support(device);
            swapchain_adequate = !(details.formats.empty() || details.present_modes.empty());
        }
        std::set<bool> requirements_met = {
            renderer::find_queue_families(device).complete(),
            extensions_supported,
            swapchain_adequate,
        };
        for (bool met : requirements_met) {
            if (!met) {
                return false;
            }
        }
        return true;
    }
    static void pick_physical_device() {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(renderer_data.instance, &device_count, nullptr);
        if (device_count == 0) {
            throw std::runtime_error("no GPUs are installed on this system with Vulkan support!");
        }
        std::vector<VkPhysicalDevice> physical_devices(device_count);
        vkEnumeratePhysicalDevices(renderer_data.instance, &device_count, physical_devices.data());
        for (auto device : physical_devices) {
            if (is_device_suitable(device)) {
                VkPhysicalDeviceProperties properties;
                vkGetPhysicalDeviceProperties(device, &properties);
                spdlog::info("chose physical device: {0}", properties.deviceName);
                uint32_t major, minor, patch;
                parse_vulkan_version(properties.driverVersion, major, minor, patch);
                spdlog::info("\tdriver version: {0}.{1}.{2}", major, minor, patch);
                parse_vulkan_version(properties.apiVersion, major, minor, patch);
                spdlog::info("\tapi version: {0}.{1}.{2}", major, minor, patch);
                renderer_data.physical_device = device;
                return;
            }
        }
        throw std::runtime_error("no suitable GPU was found!");
    }
    static void create_logical_device() {
        auto indices = renderer::find_queue_families(renderer_data.physical_device);
        std::vector<VkDeviceQueueCreateInfo> queue_create_info;
        float queue_priority = 1.f;
        std::set<uint32_t> unique_queue_families = { *indices.graphics_family, *indices.present_family };
        for (uint32_t queue_family : unique_queue_families) {
            VkDeviceQueueCreateInfo create_info;
            util::zero(create_info);
            create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            create_info.queueFamilyIndex = queue_family;
            create_info.queueCount = 1;
            create_info.pQueuePriorities = &queue_priority;
            queue_create_info.push_back(create_info);
        }
        VkDeviceCreateInfo create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = queue_create_info.size();
        create_info.pQueueCreateInfos = queue_create_info.data();
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(renderer_data.physical_device, &features);
        create_info.pEnabledFeatures = &features;
        std::vector<const char*> layer_names, extensions;
        for (const auto& layer_name : renderer_data.layer_names) {
            layer_names.push_back(layer_name.c_str());
        }
        for (const auto& extension : renderer_data.device_extensions) {
            extensions.push_back(extension.c_str());
        }
        if (!layer_names.empty()) {
            create_info.enabledLayerCount = layer_names.size();
            create_info.ppEnabledLayerNames = layer_names.data();
        }
        if (!extensions.empty()) {
            create_info.enabledExtensionCount = extensions.size();
            create_info.ppEnabledExtensionNames = extensions.data();
        }
        if (vkCreateDevice(renderer_data.physical_device, &create_info, nullptr, &renderer_data.device) != VK_SUCCESS) {
            throw std::runtime_error("could not create a logical device!");
        }
        vkGetDeviceQueue(renderer_data.device, *indices.graphics_family, 0, &renderer_data.graphics_queue);
        vkGetDeviceQueue(renderer_data.device, *indices.present_family, 0, &renderer_data.present_queue);
    }
    static void create_descriptor_pool() {
        std::vector<VkDescriptorPoolSize> pool_sizes = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        VkDescriptorPoolCreateInfo create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        create_info.poolSizeCount = pool_sizes.size();
        create_info.pPoolSizes = pool_sizes.data();
        create_info.maxSets = 1000 * pool_sizes.size();
        create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        if (vkCreateDescriptorPool(renderer_data.device, &create_info, nullptr, &renderer_data.descriptor_pool) != VK_SUCCESS) {
            throw std::runtime_error("could not create descriptor pool!");
        }
    }
    static void create_graphics_command_pool() {
        auto indices = renderer::find_queue_families(renderer_data.physical_device);
        VkCommandPoolCreateInfo create_info;
        util::zero(create_info);
        create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        create_info.queueFamilyIndex = *indices.graphics_family;
        create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(renderer_data.device, &create_info, nullptr, &renderer_data.graphics_command_pool) != VK_SUCCESS) {
            throw std::runtime_error("could not create command pool!");
        }
    }
    static void create_sync_objects() {
        VkSemaphoreCreateInfo semaphore_create_info;
        util::zero(semaphore_create_info);
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fence_create_info;
        util::zero(fence_create_info);
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (auto& frame_data : renderer_data.frame_sync_objects) {
            if (vkCreateSemaphore(renderer_data.device, &semaphore_create_info, nullptr, &frame_data.image_available_semaphore) != VK_SUCCESS ||
                vkCreateSemaphore(renderer_data.device, &semaphore_create_info, nullptr, &frame_data.render_finished_semaphore) != VK_SUCCESS ||
                vkCreateFence(renderer_data.device, &fence_create_info, nullptr, &frame_data.fence) != VK_SUCCESS) {
                throw std::runtime_error("could not create sync objects!");
            }
        }
    }
    void renderer::init(std::shared_ptr<window> _window) {
        spdlog::info("initializing renderer...");
        renderer_data.application_window = _window;
        choose_extensions();
        create_instance();
        create_debug_messenger();
        create_window_surface();
        pick_physical_device();
        create_logical_device();
        create_descriptor_pool();
        create_graphics_command_pool();
        create_sync_objects();
        image_data white_data;
        int32_t channels = 3;
        white_data.data.resize(channels, 1);
        white_data.channels = channels;
        white_data.width = white_data.height = 1;
        renderer_data.white_texture = std::make_shared<texture>(std::make_shared<image>(white_data));
    }
    static void shutdown_renderer() {
        spdlog::info("shutting down renderer...");
        vkDeviceWaitIdle(renderer_data.device);
        for (const auto& frame_data : renderer_data.frame_sync_objects) {
            vkDestroyFence(renderer_data.device, frame_data.fence, nullptr);
            vkDestroySemaphore(renderer_data.device, frame_data.render_finished_semaphore, nullptr);
            vkDestroySemaphore(renderer_data.device, frame_data.image_available_semaphore, nullptr);
        }
        vkDestroyCommandPool(renderer_data.device, renderer_data.graphics_command_pool, nullptr);
        vkDestroyDescriptorPool(renderer_data.device, renderer_data.descriptor_pool, nullptr);
        vkDestroyDevice(renderer_data.device, nullptr);
        if (renderer_data.debug_messenger != nullptr) {
            auto fpDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(renderer_data.instance, "vkDestroyDebugUtilsMessengerEXT");
            if (fpDestroyDebugUtilsMessengerEXT != nullptr) {
                fpDestroyDebugUtilsMessengerEXT(renderer_data.instance, renderer_data.debug_messenger, nullptr);
            } else {
                spdlog::warn("created debug messenger but could not destroy it - will result in memory leak");
            }
        }
        vkDestroySurfaceKHR(renderer_data.instance, renderer_data.window_surface, nullptr);
        vkDestroyInstance(renderer_data.instance, nullptr);
        renderer_data.application_window.reset();
    }
    void renderer::shutdown() {
        renderer_data.white_texture.reset();
        vkDeviceWaitIdle(renderer_data.device);
        renderer_data.should_shutdown = true;
        if (renderer_data.ref_count == 0) {
            shutdown_renderer();
        }
    }
    void renderer::new_frame() {
        renderer_data.current_frame = (renderer_data.current_frame + 1) % max_frame_count;
    }
    void renderer::add_ref() {
        renderer_data.ref_count++;
    }
    void renderer::remove_ref() {
        renderer_data.ref_count--;
        if (renderer_data.ref_count == 0 && renderer_data.should_shutdown) {
            shutdown_renderer();
        }
    }
    void renderer::render(std::shared_ptr<command_buffer> cmdbuffer, entity to_render) {
        if (!to_render.has_component<transform_component>() ||
            !to_render.has_component<model_component>()) {
            throw std::runtime_error("the given entity does not have necessary components for rendering!");
        }
        // todo: render model
    }
    std::shared_ptr<command_buffer> renderer::create_render_command_buffer() {
        auto instance = new command_buffer(renderer_data.graphics_command_pool, renderer_data.graphics_queue, false, true);
        return std::shared_ptr<command_buffer>(instance);
    }
    std::shared_ptr<command_buffer> renderer::create_single_time_command_buffer() {
        auto instance = new command_buffer(renderer_data.graphics_command_pool, renderer_data.graphics_queue, true, false);
        return std::shared_ptr<command_buffer>(instance);
    }
    std::shared_ptr<window> renderer::get_window() { return renderer_data.application_window; }
    VkInstance renderer::get_instance() { return renderer_data.instance; }
    VkPhysicalDevice renderer::get_physical_device() { return renderer_data.physical_device; }
    VkDevice renderer::get_device() { return renderer_data.device; }
    VkQueue renderer::get_graphics_queue() { return renderer_data.graphics_queue; }
    VkQueue renderer::get_present_queue() { return renderer_data.present_queue; }
    VkSurfaceKHR renderer::get_window_surface() { return renderer_data.window_surface; }
    VkDescriptorPool renderer::get_descriptor_pool() { return renderer_data.descriptor_pool; }
    std::shared_ptr<texture> renderer::get_white_texture() { return renderer_data.white_texture; }
    queue_family_indices renderer::find_queue_families(VkPhysicalDevice device) {
        queue_family_indices indices;
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());
        for (uint32_t i = 0; i < queue_families.size(); i++) {
            const auto& queue_family = queue_families[i];
            if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphics_family = i;
            }
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, renderer_data.window_surface, &present_support);
            if (present_support) {
                indices.present_family = i;
            }
            if (indices.complete()) {
                break;
            }
        }
        return indices;
    }
    swapchain_support_details renderer::query_swapchain_support(VkPhysicalDevice device) {
        swapchain_support_details details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, renderer_data.window_surface, &details.capabilities);
        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, renderer_data.window_surface, &format_count, nullptr);
        if (format_count > 0) {
            details.formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, renderer_data.window_surface, &format_count, details.formats.data());
        }
        uint32_t present_mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, renderer_data.window_surface, &present_mode_count, nullptr);
        if (format_count > 0) {
            details.present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, renderer_data.window_surface, &present_mode_count, details.present_modes.data());
        }
        return details;
    }
    const sync_objects& renderer::get_sync_objects(size_t frame_index) {
        return renderer_data.frame_sync_objects[frame_index];
    }
    size_t renderer::get_current_frame() {
        return renderer_data.current_frame;
    }
};