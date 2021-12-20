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
#include "image.h"
#include "buffers.h"
#include "pipeline.h"
#include "texture.h"
#include "command_buffer.h"
namespace vkrollercoaster {
    class skybox : public ref_counted {
    public:
        static void init();
        static void shutdown();

        skybox(ref<image_cube> skybox_texture);
        ~skybox() = default;

        skybox(const skybox&) = delete;
        skybox& operator=(const skybox&) = delete;

        void render(ref<command_buffer> cmdbuffer, bool bind_pipeline = true);

    private:
        void create_irradiance_map();

        // skybox render call objects
        ref<uniform_buffer> m_uniform_buffer;
        ref<pipeline> m_pipeline;
        ref<texture> m_skybox;

        // pbr textures
        ref<texture> m_irradiance_map;
    };
} // namespace vkrollercoaster