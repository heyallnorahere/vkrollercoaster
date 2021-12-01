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
#include "buffers.h"
#include "scene.h"
namespace vkrollercoaster {
    enum class light_type {
        spotlight,
        point,
        directional
    };
    class light : public ref_counted {
    public:
        static void init();
        static void shutdown();
        static ref<uniform_buffer> get_buffer(const std::string& shader_name);
        static void reset_buffers();
        light() = default;
        virtual ~light() = default;
        void update_buffers();
        glm::vec3& color() { return this->m_color; }
        virtual light_type get_type() = 0;
    protected:
        using set_callback_t = std::function<void(const std::string&, const void*, size_t, bool)>;
        virtual void update_typed_light_data(set_callback_t set) = 0;
    private:
        std::vector<entity> m_entities;
        glm::vec3 m_color = glm::vec3(1.f);
        friend class scene;
    };
}