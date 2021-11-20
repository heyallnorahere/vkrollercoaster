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
#include "shader.h"
#include "swapchain.h"
namespace vkrollercoaster {
    enum class vertex_attribute_type {
        FLOAT,
        INT,
        VEC2,
        IVEC2,
        VEC3,
        IVEC3,
        VEC4,
        IVEC4
    };
    struct vertex_attribute {
        vertex_attribute_type type;
        size_t stride, offset;
    };
    class pipeline {
    public:
        pipeline(std::shared_ptr<swapchain> _swapchain, std::shared_ptr<shader> _shader, const std::vector<vertex_attribute>& attributes);
        ~pipeline();    
        pipeline(const pipeline&) = delete;
        pipeline& operator=(const pipeline&) = delete;
    private:
        void create();
        void destroy();
        std::vector<vertex_attribute> m_vertex_attributes;
        std::shared_ptr<swapchain> m_swapchain;
        std::shared_ptr<shader> m_shader;
        friend class swapchain;
        friend class shader;
    };
}