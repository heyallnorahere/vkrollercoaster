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
#include "imgui_controller.h"
#define EXPOSE_RENDERER_INTERNALS
#include "renderer.h"
#include "util.h"
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>
namespace vkrollercoaster {
    imgui_controller::imgui_controller(ref<swapchain> _swapchain) {
        this->m_swapchain = _swapchain;
        renderer::add_ref();

        // initialize core imgui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        // setup imgui style
        ImGui::StyleColorsDark();
        // todo: custom imgui style
        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.f;
            style.Colors[ImGuiCol_WindowBg].w = 1.f;
        }

        // init glfw (window) backend
        GLFWwindow* glfw_window = this->m_swapchain->get_window()->get();
        ImGui_ImplGlfw_InitForVulkan(glfw_window, true);

        // init vulkan (rendering) backend
        ImGui_ImplVulkan_InitInfo init_info;
        util::zero(init_info);
        init_info.Instance = renderer::get_instance();
        VkPhysicalDevice physical_device = renderer::get_physical_device();
        init_info.PhysicalDevice = physical_device;
        init_info.Device = renderer::get_device();
        init_info.QueueFamily = *renderer::find_queue_families(physical_device).graphics_family;
        init_info.Queue = renderer::get_graphics_queue();
        init_info.PipelineCache = nullptr;
        init_info.DescriptorPool = renderer::get_descriptor_pool();
        auto support_details = renderer::query_swapchain_support(physical_device);
        init_info.MinImageCount = support_details.capabilities.minImageCount;
        init_info.ImageCount = this->m_swapchain->get_swapchain_images().size();
        ImGui_ImplVulkan_Init(&init_info, this->m_swapchain->get_render_pass());

        // todo: load type faces

        // create font atlas
        auto cmdbuffer = renderer::create_single_time_command_buffer();
        cmdbuffer->begin();
        ImGui_ImplVulkan_CreateFontsTexture(cmdbuffer->get());
        cmdbuffer->end();
        cmdbuffer->submit();
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }
    imgui_controller::~imgui_controller() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        renderer::remove_ref();
    }
    void imgui_controller::new_frame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }
    void imgui_controller::render(ref<command_buffer> cmdbuffer) {
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdbuffer->get());

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }
}