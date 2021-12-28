#include "pch.h"
#include "imgui_extensions.h"
namespace ImGui {
    static struct {
        std::unordered_map<std::string, ImFont*> fonts;
    } font_data;

    static void load_font(const std::string& name, const fs::path& path) {
        std::string string_path = path.string();

        ImGuiIO& io = ImGui::GetIO();
        font_data.fonts[name] = io.Fonts->AddFontFromFileTTF(string_path.c_str(), 16.f);
    }
    void LoadApplicationFonts() {
        ImGuiIO& io = ImGui::GetIO();

        load_font("default", "assets/fonts/Roboto-Medium.ttf");
        load_font("monospace", "assets/fonts/RobotoMono-Medium.ttf");
    }

    void InputPath(const char* label, fs::path* path, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) {
        ImGui::PushID(label);

        std::string data = path->string();
        ImGui::PushFont(font_data.fonts["monospace"]);
        if (ImGui::InputText("##input-path", &data, flags, callback, user_data)) {
            *path = data;
        }
        ImGui::PopFont();

        std::string string_label = label;
        size_t separator_pos = string_label.find("##");
        if (separator_pos != std::string::npos) {
            string_label = string_label.substr(0, separator_pos);
        }
        if (string_label.length() > 0) {
            ImGui::SameLine();
            ImGui::Text("%s", label);
        }

        ImGui::PopID();
    }
};