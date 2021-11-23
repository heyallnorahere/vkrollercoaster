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

#pragma once
namespace vkrollercoaster {
    enum class shader_stage {
        vertex,
        fragment,
        geometry,
        compute
    };
    enum class shader_language {
        glsl,
        hlsl
    };
    enum class shader_resource_type {
        uniformbuffer,
        storagebuffer,
        sampledimage,
        pushconstantbuffer
    };
    struct shader_field {
        size_t offset, type;
    };
    struct shader_reflection_data;
    struct shader_type {
        size_t find_offset(const std::string& field_name, const shader_reflection_data& base_data) const;
        std::string name;
        size_t size, array_stride, array_size;
        std::map<std::string, shader_field> fields;
    };
    struct shader_resource_data {
        std::string name;
        shader_resource_type resource_type;
        shader_stage stage;
        size_t type;
    };
    struct shader_reflection_data {
        bool find_resource(const std::string& name, uint32_t& set, uint32_t& binding) const;
        std::map<uint32_t, std::map<uint32_t, shader_resource_data>> resources;
        std::vector<shader_type> types;
    };
    class pipeline;
    class shader {
    public:
        static VkShaderStageFlagBits get_stage_flags(shader_stage stage);
        shader(const fs::path& source);
        shader(const fs::path& source, shader_language language);
        ~shader();
        shader(const shader&) = delete;
        shader& operator=(const shader&) = delete;
        void reload();
        shader_reflection_data& get_reflection_data() { return this->m_reflection_data; }
        const std::vector<VkPipelineShaderStageCreateInfo>& get_pipeline_info() { return this->m_shader_data; }
    private:
        void create();
        void compile(std::map<shader_stage, std::vector<uint32_t>>& spirv);
        void reflect(const std::vector<uint32_t>& spirv, shader_stage stage);
        void destroy();
        std::vector<VkPipelineShaderStageCreateInfo> m_shader_data;
        shader_language m_language;
        fs::path m_path;
        shader_reflection_data m_reflection_data;
        std::unordered_set<pipeline*> m_dependents;
        friend class pipeline;
    };
}