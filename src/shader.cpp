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
#include "shader.h"
#include "renderer.h"
#include "util.h"
#include <shaderc/shaderc.hpp>
namespace vkrollercoaster {
    VkShaderStageFlagBits shader::get_stage_flags(shader_stage stage) {
        switch (stage) {
        case shader_stage::vertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case shader_stage::fragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case shader_stage::geometry:
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        case shader_stage::compute:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        default:
            throw std::runtime_error("invalid shader stage");
        }
    }
    static std::map<std::string, shader_language> language_map = {
        { ".glsl", shader_language::glsl },
        { ".hlsl", shader_language::hlsl }
    };
    static shader_language determine_language(const fs::path& path) {
        auto extension = path.extension().string();
        if (language_map.find(extension) == language_map.end()) {
            throw std::runtime_error("invalid shader extension: " + extension);
        }
        return language_map[extension];
    }
    shader::shader(const fs::path& path) : shader(path, determine_language(path)) { }
    shader::shader(const fs::path& path, shader_language language) {
        this->m_path = path;
        this->m_language = language;
        renderer::add_ref();
        this->create();
    }
    shader::~shader() {
        this->destroy();
        renderer::remove_ref();
    }
    void shader::reload() {
        this->destroy();
        this->create();
    }
    void shader::create() {
        std::map<shader_stage, std::vector<uint32_t>> spirv;
        this->compile(spirv);
        VkDevice device = renderer::get_device();
        for (const auto& [stage, data] : spirv) {
            // todo: reflect
            VkShaderModuleCreateInfo module_create_info;
            util::zero(module_create_info);
            module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_create_info.codeSize = data.size() * sizeof(uint32_t);
            module_create_info.pCode = data.data();
            auto& stage_create_info = this->m_shader_data.emplace_back();
            util::zero(stage_create_info);
            stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            if (vkCreateShaderModule(device, &module_create_info, nullptr, &stage_create_info.module) != VK_SUCCESS) {
                throw std::runtime_error("could not create shader module!");
            }
            stage_create_info.pName = "main";
            stage_create_info.stage = get_stage_flags(stage);
        }
    }
    static std::map<std::string, shader_stage> stage_map = {
        { "vertex", shader_stage::vertex },
        { "fragment", shader_stage::fragment },
        { "pixel", shader_stage::fragment },
        { "geometry", shader_stage::geometry },
        { "compute", shader_stage::compute }
    };
    void shader::compile(std::map<shader_stage, std::vector<uint32_t>>& spirv) {
        // todo: shader cache
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        shaderc_source_language source_language;
        switch (this->m_language) {
        case shader_language::glsl:
            source_language = shaderc_source_language_glsl;
            break;
        case shader_language::hlsl:
            source_language = shaderc_source_language_hlsl;
            break;
        default:
            throw std::runtime_error("invalid shader language!");
        }
        options.SetSourceLanguage(source_language);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
        options.SetWarningsAsErrors();
        options.SetGenerateDebugInfo();
        std::map<shader_stage, std::stringstream> sources;
        {
            std::stringstream file_data(util::read_file(this->m_path));
            std::string line;
            std::optional<shader_stage> current_stage;
            std::string stage_switch = "#stage ";
            while (std::getline(file_data, line)) {
                if (line.substr(0, stage_switch.length()) == stage_switch) {
                    std::string stage_string = line.substr(stage_switch.length());
                    if (stage_map.find(stage_string) == stage_map.end()) {
                        throw std::runtime_error(this->m_path.string() + ": invalid shader stage: " + stage_string);
                    }
                    current_stage = stage_map[stage_string];
                } else {
                    if (!current_stage.has_value()) {
                        spdlog::warn("{0}: no stage specified - assuming compute", this->m_path.string());
                        current_stage = shader_stage::compute;
                    }
                    shader_stage stage = *current_stage;
                    sources[stage] << line << '\n';
                }
            }
        }
        for (const auto& [stage, stream] : sources) {
            auto source = stream.str();
            shaderc_shader_kind shaderc_stage;
            switch (stage) {
            case shader_stage::vertex:
                shaderc_stage = shaderc_vertex_shader;
                break;
            case shader_stage::fragment:
                shaderc_stage = shaderc_fragment_shader;
                break;
            case shader_stage::geometry:
                shaderc_stage = shaderc_geometry_shader;
                break;
            case shader_stage::compute:
                shaderc_stage = shaderc_compute_shader;
                break;
            default:
                throw std::runtime_error("invalid shader stage!");
            }
            std::string path = this->m_path.string();
            auto result = compiler.CompileGlslToSpv(source, shaderc_stage, path.c_str());
            if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
                throw std::runtime_error("could not compile shader: " + result.GetErrorMessage());
            }
            spirv[stage] = std::vector<uint32_t>(result.cbegin(), result.cend());
        }
    }
    void shader::destroy() {
        VkDevice device = renderer::get_device();
        for (const auto& stage : this->m_shader_data) {
            vkDestroyShaderModule(device, stage.module, nullptr);
        }
        this->m_shader_data.clear();
    }
}