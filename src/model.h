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
#include "material.h"
#include "buffers.h"
#include <assimp/Importer.hpp>
struct aiScene;
struct aiNode;
struct aiMesh;
namespace vkrollercoaster {
    struct vertex {
        glm::vec3 position, normal;
        glm::vec2 uv;
        glm::vec3 tangent;
    };
    class model;

    // a "model source" represents a file on disk
    class model_source : public ref_counted {
    public:
        struct mesh {
            size_t vertex_offset, vertex_count, index_offset, index_count, material_index;
            aiNode* node;
            aiMesh* assimp_mesh;
        };
        model_source(const fs::path& path);
        model_source(const model_source&) = delete;
        model_source& operator=(const model_source&) = delete;
        void reload();

        const std::vector<vertex>& get_vertices() { return this->m_vertices; }
        const std::vector<uint32_t>& get_indices() { return this->m_indices; }
        const std::vector<mesh>& get_meshes() { return this->m_meshes; }
        const std::vector<ref<material>>& get_materials() { return this->m_materials; }
        const fs::path& get_path() { return this->m_path; }

    private:
        void process_node(aiNode* node, const void* parent_transform);
        void process_mesh(aiMesh* mesh_, aiNode* node, const void* node_transform);
        void process_materials();
        fs::path get_resource_path(const aiString& ai_path);

        std::vector<vertex> m_vertices;
        std::vector<uint32_t> m_indices;
        std::vector<mesh> m_meshes;
        std::vector<ref<material>> m_materials;

        fs::path m_path;
        const aiScene* m_scene;
        std::unique_ptr<Assimp::Importer> m_importer;

        std::set<model*> m_created_models;
        friend class model;
    };

    // a mesh to be rendered
    class model : public ref_counted {
    public:
        struct mesh {
            size_t index_offset, index_count;
            size_t material_index;
        };
        struct model_data {
            std::vector<ref<material>> materials;
            std::vector<mesh> meshes;
            std::vector<vertex> vertices;
            std::vector<uint32_t> indices;
        };
        struct buffer_data {
            ref<vertex_buffer> vertices;
            std::map<size_t, ref<index_buffer>> indices;
        };

        model(ref<model_source> source);
        model(const model_data& data);
        ~model();
        model(const model&) = delete;
        model& operator=(const model&) = delete;

        void set_data(const model_data& data);

        ref<model_source> get_source() { return this->m_source; }
        const std::vector<vertex>& get_vertices() { return this->m_vertices; }
        const std::vector<uint32_t>& get_indices() { return this->m_indices; }
        const std::vector<mesh>& get_meshes() { return this->m_meshes; }
        const std::vector<ref<material>>& get_materials() { return this->m_materials; }
        const vertex_input_data& get_input_layout() { return this->m_input_layout; }
        const buffer_data& get_buffers() { return this->m_buffers; }

    private:
        void set_input_layout();
        void acquire_mesh_data();
        void invalidate_buffers();

        std::vector<vertex> m_vertices;
        std::vector<uint32_t> m_indices;
        std::vector<mesh> m_meshes;
        std::vector<ref<material>> m_materials;
        buffer_data m_buffers;
        vertex_input_data m_input_layout;

        ref<model_source> m_source;
        friend class model_source;
    };
} // namespace vkrollercoaster