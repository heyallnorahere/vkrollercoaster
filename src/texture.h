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
#include "pipeline.h"
namespace vkrollercoaster {
    class texture : public ref_counted {
    public:
        texture(ref<image> _image);
        ~texture();
        texture(const texture&) = delete;
        texture& operator=(const texture&) = delete;
        void bind(ref<pipeline> _pipeline, uint32_t set, uint32_t binding, uint32_t slot = 0);
        void bind(ref<pipeline> _pipeline, const std::string& name, uint32_t slot = 0);
        ref<image> get_image() { return this->m_image; }
    private:
        void create_sampler();
        ref<image> m_image;
        VkSampler m_sampler;
        std::set<pipeline*> m_bound_pipelines;
        friend class pipeline;
    };
}