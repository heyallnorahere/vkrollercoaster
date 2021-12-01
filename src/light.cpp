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
#include "light.h"
#include "shader.h"
namespace vkrollercoaster {
    static struct {
        std::unordered_map<std::string, ref<uniform_buffer>> buffers;
    } light_data;
    void light::init() {
        std::vector<std::string> shader_names;
        shader_library::get_names(shader_names);
        for (const auto& shader_name : shader_names) {
            auto _shader = shader_library::get(shader_name);
            auto& reflection_data = _shader->get_reflection_data();
            uint32_t set, binding;
            if (!reflection_data.find_resource("light_data", set, binding)) {
                continue;
            }
            auto buffer = uniform_buffer::from_shader_data(_shader, set, binding);
            light_data.buffers.insert(std::make_pair(shader_name, buffer));
        }
    }
    void light::shutdown() {
        light_data.buffers.clear();
    }
    ref<uniform_buffer> light::get_buffer(const std::string& shader_name) {
        ref<uniform_buffer> buffer;
        if (light_data.buffers.find(shader_name) != light_data.buffers.end()) {
            buffer = light_data.buffers[shader_name];
        }
        return buffer;
    }
}