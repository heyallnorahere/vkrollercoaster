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
#include "render_target.h"
#include "command_buffer.h"
namespace vkrollercoaster {
    enum class vertex_attribute_type {
        FLOAT,
        INT,
        VEC2,
        IVEC2,
        VEC3,
        IVEC3,
        VEC4,
        IVEC4,
        BOOLEAN,
    };
    struct vertex_attribute {
        vertex_attribute_type type;
        size_t offset;
    };
    struct vertex_input_data {
        size_t stride = 0;
        std::vector<vertex_attribute> attributes;
    };
    enum class pipeline_polygon_mode { fill, wireframe };
    enum class pipeline_front_face { clockwise, counter_clockwise };
    struct pipeline_spec {
        pipeline_spec() = default;
        bool enable_depth_testing = true;
        bool enable_blending = true;
        pipeline_polygon_mode polygon_mode = pipeline_polygon_mode::fill;
        pipeline_front_face front_face = pipeline_front_face::clockwise;
        vertex_input_data input_layout;
    };
    class uniform_buffer;
    class texture;
    class material;
    class pipeline : public ref_counted {
    public:
        struct descriptor_set {
            VkDescriptorSetLayout layout;
            std::vector<VkDescriptorSet> sets;
        };
        pipeline(ref<render_target> target, ref<shader> _shader, const pipeline_spec& spec);
        ~pipeline();
        pipeline(const pipeline&) = delete;
        pipeline& operator=(const pipeline&) = delete;
        void bind(ref<command_buffer> cmdbuffer);
        void reload(bool descriptor_sets = false);
        ref<shader> get_shader() { return this->m_shader; }
        ref<render_target> get_render_target() { return this->m_render_target; }
        VkPipeline get() { return this->m_pipeline; }
        VkPipelineLayout get_layout() { return this->m_layout; }
        VkViewport get_viewport() { return this->m_viewport; }
        VkRect2D get_scissor() { return this->m_scissor; }
        const std::map<uint32_t, descriptor_set>& get_descriptor_sets() {
            return this->m_descriptor_sets;
        }
        pipeline_spec& spec() { return this->m_spec; }

    private:
        enum class buffer_type {
            ubo,
            // todo: shader storage buffer object
        };
        struct bound_buffer_desc {
            buffer_type type;
            void* object = nullptr;
        };
        struct texture_binding_desc {
            uint32_t set, binding, slot;
            // why is this necessary???
            bool operator==(const texture_binding_desc& other) const {
                return (this->set == other.set) && (this->binding == other.binding) &&
                       (this->slot) == (other.slot);
            }
            bool operator!=(const texture_binding_desc& other) const { return !(*this == other); }
            struct hash {
                size_t operator()(const texture_binding_desc& desc) const {
                    std::hash<uint32_t> hasher;
                    return (hasher(desc.set) << 2) ^ (hasher(desc.binding) << 1) ^
                           hasher(desc.slot);
                }
            };
        };
        void create_descriptor_sets();
        void create_pipeline();
        void destroy_pipeline();
        void destroy_descriptor_sets();
        void rebind_objects();
        pipeline_spec m_spec;
        ref<render_target> m_render_target;
        ref<shader> m_shader;
        VkViewport m_viewport;
        VkRect2D m_scissor;
        VkPipelineLayout m_layout;
        VkPipeline m_pipeline;
        std::map<uint32_t, descriptor_set> m_descriptor_sets;
        std::vector<VkPushConstantRange> m_push_constant_ranges;
        std::unordered_map<texture_binding_desc, texture*, texture_binding_desc::hash>
            m_bound_textures;
        std::map<uint32_t, std::map<uint32_t, bound_buffer_desc>> m_bound_buffers;
        material* m_material;
        friend class swapchain;
        friend class shader;
        friend class uniform_buffer;
        friend class texture;
        friend class material;
    };
} // namespace vkrollercoaster