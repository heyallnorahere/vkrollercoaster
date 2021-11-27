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
#include "swapchain.h"
#include "texture.h"
#include "shader.h"
#include "buffers.h"
#include "pipeline.h"
namespace vkrollercoaster {
    class material {
    public:
        static void init(std::shared_ptr<swapchain> swap_chain);
        static void shutdown();
        material(std::shared_ptr<shader> _shader);
        ~material() = default;
        std::shared_ptr<pipeline> create_pipeline(const pipeline_spec& spec);
        void set(const std::string& name, int32_t data);
        void set(const std::string& name, uint32_t data);
        void set(const std::string& name, float data);
        void set(const std::string& name, const glm::vec3& data);
        void set(const std::string& name, const glm::vec4& data);
        void set(const std::string& name, const glm::mat4& data);
        void set(const std::string& name, std::shared_ptr<texture> tex, uint32_t slot = 0);
    private:
        std::shared_ptr<swapchain> m_swapchain;
        std::shared_ptr<uniform_buffer> m_buffer;
        std::shared_ptr<shader> m_shader;
        std::map<std::string, std::vector<std::shared_ptr<texture>>> m_textures;
        uint32_t m_set, m_binding;
    };
}