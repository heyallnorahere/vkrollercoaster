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
#include "menus.h"
#include "renderer.h"
namespace vkrollercoaster {
    void renderer_info::update() {
        ImGui::Begin("Renderer info", &this->m_open);
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("FPS: %f", io.Framerate);
        if (ImGui::Button("Reload shaders")) {
            std::vector<std::string> names;
            shader_library::get_names(names);
            for (const auto& name : names) {
                shader_library::get(name)->reload();
            }
        }
        if (ImGui::CollapsingHeader("Device info")) {
            VkPhysicalDevice physical_device = renderer::get_physical_device();
            VkPhysicalDeviceProperties device_properties;
            vkGetPhysicalDeviceProperties(physical_device, &device_properties);
            ImGui::Text("Selected device: %s", device_properties.deviceName);
            ImGui::Indent();

            uint32_t major, minor, patch;
            renderer::expand_vulkan_version(device_properties.apiVersion, major, minor, patch);

            std::string vendor;
            switch (device_properties.vendorID) {
            case 0x1002: vendor = "AMD"; break;
            case 0x10DE: vendor = "NVIDIA"; break;
            case 0x8086: vendor = "Intel"; break;
            case 0x13B5: vendor = "ARM"; break;
            default: vendor = "unknown"; break;
            }

            std::string device_type;
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
        }
        ImGui::End();
    }
}