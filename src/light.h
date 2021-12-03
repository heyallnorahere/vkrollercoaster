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
    struct attentuation_settings;
    class light : public ref_counted {
    public:
        static void init();
        static void shutdown();
        static ref<uniform_buffer> get_buffer(const std::string& shader_name);
        static void reset_buffers();
        light() = default;
        virtual ~light() = default;
        glm::vec3& diffuse_color() { return this->m_diffuse_color; }
        glm::vec3& specular_color() { return this->m_specular_color; }
        glm::vec3& ambient_color() { return this->m_ambient_color; }
        virtual light_type get_type() = 0;
    protected:
        using set_callback_t = std::function<void(const std::string&, const void*, size_t, bool)>;
        virtual void update_typed_light_data(set_callback_t set) = 0;
    private:
        void update_buffers(const std::vector<entity>& entities);
        glm::vec3 m_diffuse_color = glm::vec3(0.8f);
        glm::vec3 m_specular_color = glm::vec3(1.f);
        glm::vec3 m_ambient_color = glm::vec3(0.05f);
        friend class scene;
        friend struct attenuation_settings;
    };
    // for ui shenanigans
    struct attenuation_value {
        attenuation_value(float value) : value(value), edit(false) { }
        float value;
        bool edit;
    };
    struct attenuation_settings {
        attenuation_value constant = attenuation_value(1.f);
        attenuation_value linear = attenuation_value(0.7f);
        attenuation_value quadratic = attenuation_value(1.8f);
        float target_distance = 7.f;
        void update(light::set_callback_t set);
    };
    class point_light : public light {
    public:
        point_light(const attenuation_settings& attenuation = attenuation_settings());
        virtual ~point_light() override = default;
        attenuation_settings& attenuation() { return this->m_attenuation; }
        virtual light_type get_type() override { return light_type::point; }
    protected:
        virtual void update_typed_light_data(set_callback_t set) override;
    private:
        attenuation_settings m_attenuation;
    };
    class spotlight : public light {
    public:
        spotlight(const glm::vec3& direction, float cutoff, float outer_cutoff, const attenuation_settings& attenuation = attenuation_settings());
        virtual ~spotlight() override = default;
        attenuation_settings& attenuation() { return this->m_attenuation; }
        glm::vec3& direction() { return this->m_direction; }
        float& cutoff() { return this->m_cutoff; }
        float& outer_cutoff() { return this->m_outer_cutoff; }
        virtual light_type get_type() override { return light_type::spotlight; }
    protected:
        virtual void update_typed_light_data(set_callback_t set) override;
    private:
        attenuation_settings m_attenuation;
        glm::vec3 m_direction;
        float m_cutoff, m_outer_cutoff;
    };
}