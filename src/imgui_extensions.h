#pragma omce
namespace ImGui {
    // loads fonts in assets/fonts/
    void LoadApplicationFonts();

    // allows input for a std::filesystem::path string
    void InputPath(const char* label, fs::path* path, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);
};