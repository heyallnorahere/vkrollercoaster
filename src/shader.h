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
    private:
        void create();
        void compile(std::map<shader_stage, std::vector<uint32_t>>& spirv);
        void destroy();
        std::vector<VkPipelineShaderStageCreateInfo> m_shader_data;
        shader_language m_language;
        fs::path m_path;
        std::unordered_set<pipeline*> m_dependents;
        friend class pipeline;
    };
}