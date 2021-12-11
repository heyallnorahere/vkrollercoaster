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
#include "menus/menus.h"
#include "application.h"
#include "input_manager.h"
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>
namespace vkrollercoaster {
    static struct {
        ref<input_manager> im;
        ref<swapchain> swap_chain;
        std::vector<ref<menu>> menus;
        uint32_t dependent_count = 0;
        bool should_shutdown = false;
        bool show_demo_window = false;
    } imgui_data;
    static void set_style() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.Colors[ImGuiCol_Text]                   = ImVec4(0.86f, 0.93f, 0.89f, 0.78f);
        style.Colors[ImGuiCol_TextDisabled]           = ImVec4(0.86f, 0.93f, 0.89f, 0.28f);
        style.Colors[ImGuiCol_WindowBg]               = ImVec4(0.03f, 0.03f, 0.04f, 1.00f);
        style.Colors[ImGuiCol_ChildBg]                = ImVec4(0.18f, 0.18f, 0.22f, 0.58f);
        style.Colors[ImGuiCol_PopupBg]                = ImVec4(0.18f, 0.18f, 0.22f, 0.90f);
        style.Colors[ImGuiCol_Border]                 = ImVec4(0.14f, 0.14f, 0.17f, 0.60f);
        style.Colors[ImGuiCol_BorderShadow]           = ImVec4(0.18f, 0.18f, 0.22f, 0.00f);
        style.Colors[ImGuiCol_FrameBg]                = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
        style.Colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.32f, 0.07f, 0.50f, 0.78f);
        style.Colors[ImGuiCol_FrameBgActive]          = ImVec4(0.32f, 0.07f, 0.50f, 1.00f);
        style.Colors[ImGuiCol_TitleBg]                = ImVec4(0.19f, 0.04f, 0.30f, 1.00f);
        style.Colors[ImGuiCol_TitleBgActive]          = ImVec4(0.51f, 0.12f, 0.80f, 1.00f);
        style.Colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.18f, 0.18f, 0.22f, 0.75f);
        style.Colors[ImGuiCol_MenuBarBg]              = ImVec4(0.18f, 0.18f, 0.22f, 0.47f);
        style.Colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.19f, 0.04f, 0.30f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.32f, 0.07f, 0.50f, 0.78f);
        style.Colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.32f, 0.07f, 0.50f, 1.00f);
        style.Colors[ImGuiCol_CheckMark]              = ImVec4(0.51f, 0.12f, 0.80f, 1.00f);
        style.Colors[ImGuiCol_SliderGrab]             = ImVec4(0.27f, 0.26f, 0.32f, 1.00f);
        style.Colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.51f, 0.12f, 0.80f, 1.00f);
        style.Colors[ImGuiCol_Button]                 = ImVec4(0.35f, 0.34f, 0.42f, 1.00f);
        style.Colors[ImGuiCol_ButtonHovered]          = ImVec4(0.32f, 0.07f, 0.50f, 1.00f);
        style.Colors[ImGuiCol_ButtonActive]           = ImVec4(0.51f, 0.12f, 0.80f, 1.00f);
        style.Colors[ImGuiCol_Header]                 = ImVec4(0.32f, 0.07f, 0.50f, 0.76f);
        style.Colors[ImGuiCol_HeaderHovered]          = ImVec4(0.32f, 0.07f, 0.50f, 0.86f);
        style.Colors[ImGuiCol_HeaderActive]           = ImVec4(0.51f, 0.12f, 0.80f, 1.00f);
        style.Colors[ImGuiCol_Separator]              = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        style.Colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
        style.Colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
        style.Colors[ImGuiCol_ResizeGrip]             = ImVec4(0.47f, 0.77f, 0.83f, 0.04f);
        style.Colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.32f, 0.07f, 0.50f, 0.78f);
        style.Colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.32f, 0.07f, 0.50f, 1.00f);
        style.Colors[ImGuiCol_Tab]                    = ImVec4(0.18f, 0.18f, 0.22f, 0.40f);
        style.Colors[ImGuiCol_TabHovered]             = ImVec4(0.51f, 0.12f, 0.80f, 1.00f);
        style.Colors[ImGuiCol_TabActive]              = ImVec4(0.32f, 0.07f, 0.50f, 1.00f);
        style.Colors[ImGuiCol_TabUnfocused]           = ImVec4(0.18f, 0.18f, 0.22f, 0.40f);
        style.Colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.18f, 0.18f, 0.22f, 0.70f);
        style.Colors[ImGuiCol_DockingPreview]         = ImVec4(0.51f, 0.12f, 0.80f, 0.30f);
        style.Colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        style.Colors[ImGuiCol_PlotLines]              = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
        style.Colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.32f, 0.07f, 0.50f, 1.00f);
        style.Colors[ImGuiCol_PlotHistogram]          = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
        style.Colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.32f, 0.07f, 0.50f, 1.00f);
        style.Colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
        style.Colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
        style.Colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
        style.Colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        style.Colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
        style.Colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.32f, 0.07f, 0.50f, 0.43f);
        style.Colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        style.Colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        style.Colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        style.Colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        style.Colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.18f, 0.18f, 0.22f, 0.73f);
    }
    void imgui_controller::init(ref<swapchain> _swapchain) {
        imgui_data.swap_chain = _swapchain;
        imgui_data.im = ref<input_manager>::create(_swapchain->get_window());
        renderer::add_ref();

        // initialize core imgui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        // setup imgui style
        ImGui::StyleColorsDark();
        set_style();
        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.f;
            style.Colors[ImGuiCol_WindowBg].w = 1.f;
        }

        // init glfw (window) backend
        GLFWwindow* glfw_window = imgui_data.swap_chain->get_window()->get();
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
        init_info.ImageCount = imgui_data.swap_chain->get_swapchain_images().size();
        ImGui_ImplVulkan_Init(&init_info, imgui_data.swap_chain->get_render_pass());

        // load fonts
        io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Medium.ttf", 16.f);

        // create font atlas
        auto cmdbuffer = renderer::create_single_time_command_buffer();
        cmdbuffer->begin();
        ImGui_ImplVulkan_CreateFontsTexture(cmdbuffer->get());
        cmdbuffer->end();
        cmdbuffer->submit();
        ImGui_ImplVulkan_DestroyFontUploadObjects();

        // add menus
        imgui_data.menus.push_back(ref<inspector>::create());
        imgui_data.menus.push_back(ref<renderer_info>::create());
        imgui_data.menus.push_back(ref<viewport>::create());
    }
    static void shutdown_imgui() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        imgui_data.swap_chain.reset();
        renderer::remove_ref();
    }
    void imgui_controller::shutdown() {
        imgui_data.menus.clear();
        imgui_data.im.reset();
        imgui_data.should_shutdown = true;
        if (imgui_data.dependent_count == 0) {
            shutdown_imgui();
        }
    }
    void imgui_controller::new_frame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }
    struct shortcut_state {
        bool exit, toggle_cursor;
    };
    static bool get_shortcut_state(int32_t key, int32_t mods) {
        auto key_state = imgui_data.im->get_key(key);
        if (key_state.down) {
            return (key_state.mods & mods) == mods;
        }
        return false;
    }
    static void handle_shortcuts(shortcut_state& state) {
        imgui_data.im->update();

        // exit
        state.exit = get_shortcut_state(GLFW_KEY_Q, GLFW_MOD_CONTROL);

        // cursor toggle
        state.toggle_cursor = get_shortcut_state(GLFW_KEY_E, GLFW_MOD_CONTROL | GLFW_MOD_SHIFT);
    }
    static void update_dockspace_menu_bar() {
        shortcut_state state;
        handle_shortcuts(state);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit", "Ctrl+Q")) {
                    state.exit = true;
                }

                if (ImGui::MenuItem("Toggle cursor visibility", "Ctrl+Shift+E")) {
                    state.toggle_cursor = true;
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                for (ref<menu> _menu : imgui_data.menus) {
                    std::string title = _menu->get_title();
                    ImGui::MenuItem(title.c_str(), "", &_menu->open());
                }

                ImGui::MenuItem("Demo window", "", &imgui_data.show_demo_window);

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        if (state.exit) {
            application::quit();
        }

        if (state.toggle_cursor) {
            if (imgui_data.im->is_cursor_enabled()) {
                imgui_data.im->disable_cursor();
            } else {
                imgui_data.im->enable_cursor();
            }
        }
    }
    static void update_dockspace() {
        // if docking isn't enabled, we can't use a dockspace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable == ImGuiConfigFlags_None) {
            return;
        }

        constexpr bool fullscreen = true;
        constexpr bool padding = false;
        
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (fullscreen) {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();

            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

            window_flags |=
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoNavFocus;
        }

        if (!padding) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        }
        ImGui::Begin("Dockspace", nullptr, window_flags);
        if (!padding) {
            ImGui::PopStyleVar();
        }

        if (fullscreen) {
            ImGui::PopStyleVar(2);
        }

        ImGuiID dockspace_id = ImGui::GetID("vkrollercoaster-dockspace");
        ImGui::DockSpace(dockspace_id); // we don't need any flags

        // submit menu bar
        update_dockspace_menu_bar();

        ImGui::End();
    }
    void imgui_controller::update_menus() {
        update_dockspace();

        if (imgui_data.show_demo_window) {
            ImGui::ShowDemoWindow(&imgui_data.show_demo_window);
        }

        for (ref<menu> _menu : imgui_data.menus) {
            if (_menu->open()) {
                _menu->update();
            }
        }
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
    void imgui_controller::add_dependent() {
        imgui_data.dependent_count++;
    }
    void imgui_controller::remove_dependent() {
        imgui_data.dependent_count--;
        if (imgui_data.dependent_count == 0 && imgui_data.should_shutdown) {
            shutdown_imgui();
        }
    }
}