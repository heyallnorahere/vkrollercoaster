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
#include "model.h"
#include "util.h"
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>
namespace vkrollercoaster {
    template<glm::length_t L, typename T> static glm::vec<L, float> convert(const T& assimp_vector) {
        glm::vec<L, float> glm_vector;
        for (size_t i = 0; i < (size_t)L; i++) {
            glm_vector[i] = assimp_vector[i];
        }
        return glm_vector;
    }
    struct error_logstream : public Assimp::LogStream {
        virtual void write(const char* message) override {
            throw std::runtime_error("assimp: " + std::string(message));
        }
    };
    struct warning_logstream : public Assimp::LogStream {
        virtual void write(const char* message) override {
            spdlog::warn("assimp: {0}", message);
        }
    };
    static void initialize_logger() {
        if (!Assimp::DefaultLogger::isNullLogger()) {
            return;
        }
        Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
        Assimp::Logger* logger = Assimp::DefaultLogger::get();
        logger->attachStream(new error_logstream, Assimp::Logger::Err);
        logger->attachStream(new warning_logstream, Assimp::Logger::Warn);
    }
    model::model(const fs::path& path) {
        initialize_logger();
        this->m_path = path;
        if (!this->m_path.has_parent_path()) {
            this->m_path = fs::current_path() / this->m_path;
        }
        this->reload();
    }
    static constexpr uint32_t import_flags =
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_GenUVCoords |
        aiProcess_OptimizeMeshes |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ValidateDataStructure |
        aiProcess_MakeLeftHanded;
    void model::reload() {
        this->m_importer = std::make_unique<Assimp::Importer>();
        this->m_vertices.clear();
        this->m_indices.clear();
        this->m_meshes.clear();
        this->m_render_call_data.clear();
        this->m_scene = this->m_importer->ReadFile(this->m_path.string(), import_flags);
        if (!this->m_scene || this->m_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !this->m_scene->mRootNode) {
            throw std::runtime_error("could not load model: " + std::string(this->m_importer->GetErrorString()));
        }
        material_map_t material_map;
        std::vector<ref<material>> materials;
        this->process_materials(materials);
        this->process_node(this->m_scene->mRootNode, material_map);
        this->create_render_call_data(material_map, materials);
    }
    void model::process_node(aiNode* node, material_map_t& material_map) {
        for (size_t i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh_ = this->m_scene->mMeshes[node->mMeshes[i]];
            this->process_mesh(mesh_, node, material_map);
        }
        for (size_t i = 0; i < node->mNumChildren; i++) {
            aiNode* child = node->mChildren[i];
            this->process_node(child, material_map);
        }
    }
    void model::process_mesh(aiMesh* mesh_, aiNode* node, material_map_t& material_map) {
        std::vector<vertex> vertices;
        std::vector<uint32_t> indices;
        for (size_t i = 0; i < mesh_->mNumVertices; i++) {
            vertex v;
            v.position = convert<3>(mesh_->mVertices[i]);
            v.normal = convert<3>(mesh_->mNormals[i]);
            if (mesh_->mTextureCoords[0] != nullptr) {
                v.uv = convert<2>(mesh_->mTextureCoords[0][i]);
            } else {
                v.uv = glm::vec2(0.f);
            }
            vertices.push_back(v);
        }
        for (size_t i = 0; i < mesh_->mNumFaces; i++) {
            aiFace face = mesh_->mFaces[i];
            if (face.mNumIndices != 3) {
                throw std::runtime_error("primitives other than triangles are not supported!");
            }
            for (size_t j = 0; j < face.mNumIndices; j++) {
                indices.push_back(face.mIndices[j]);
            }
        }
        size_t mesh_index = this->m_meshes.size();
        mesh& mesh_data = this->m_meshes.emplace_back();
        mesh_data.vertex_offset = this->m_vertices.size();
        mesh_data.index_offset = this->m_indices.size();
        mesh_data.assimp_mesh = mesh_;
        mesh_data.node = node;
        mesh_data.vertex_count = vertices.size();
        mesh_data.index_count = indices.size();
        mesh_data.material_index = mesh_->mMaterialIndex;
        material_map[mesh_data.material_index].push_back(mesh_index);
        util::append_vector(this->m_vertices, vertices);
        util::append_vector(this->m_indices, indices);
    }
    void model::process_materials(std::vector<ref<material>>& materials) {
        // todo: change when skinning
        const std::string shader_name = "default_static";
        ref<shader> material_shader = shader_library::get(shader_name);

        for (size_t i = 0; i < this->m_scene->mNumMaterials; i++) {
            auto _material = ref<material>::create(material_shader);
            aiMaterial* ai_material = this->m_scene->mMaterials[i];
            aiString ai_path;

            // albedo map
            if (ai_material->GetTexture(aiTextureType_DIFFUSE, 0, &ai_path) == aiReturn_SUCCESS) {
                auto path = this->m_path.parent_path() / fs::path(ai_path.C_Str());
                auto img = image::from_file(path);
                if (img) {
                    auto tex = ref<texture>::create(img);
                    _material->set("albedo_texture", tex);
                }
            }
            
            // shininess
            float shininess;
            if (ai_material->Get(AI_MATKEY_SHININESS, shininess) != aiReturn_SUCCESS) {
                shininess = 80.f;
            }
            _material->set("shininess", shininess);

            // albedo color
            glm::vec3 albedo_color(1.f);
            aiColor3D ai_color;
            if (ai_material->Get(AI_MATKEY_COLOR_DIFFUSE, ai_color) == aiReturn_SUCCESS) {
                albedo_color = glm::vec3(ai_color.r, ai_color.g, ai_color.b);
            }
            _material->set("albedo_color", albedo_color);

            materials.push_back(_material);
        }
    }
    void model::create_render_call_data(const material_map_t& material_map, const std::vector<ref<material>>& materials) {
        pipeline_spec spec;
        spec.input_layout.stride = sizeof(vertex);
        spec.input_layout.attributes = {
            { vertex_attribute_type::VEC3, offsetof(vertex, position) },
            { vertex_attribute_type::VEC3, offsetof(vertex, normal) },
            { vertex_attribute_type::VEC2, offsetof(vertex, uv) },
        };
        for (const auto& [material_index, mesh_indices] : material_map) {
            if (mesh_indices.empty()) {
                continue;
            }
            auto _material = materials[material_index];
            auto& render_call = this->m_render_call_data.emplace_back();
            render_call._material._material = _material;
            render_call._material._pipeline = _material->create_pipeline(spec);
            std::vector<vertex> vertices;
            std::vector<uint32_t> indices;
            for (size_t mesh_index : mesh_indices) {
                const auto& _mesh = this->m_meshes[mesh_index];
                auto begin = this->m_vertices.begin() + _mesh.vertex_offset;
                auto end = begin + _mesh.vertex_count;
                size_t vertex_offset = vertices.size();
                vertices.insert(vertices.end(), begin, end);
                for (size_t i = 0; i < _mesh.index_count; i++) {
                    size_t index_index = i + _mesh.index_offset;
                    size_t index = this->m_indices[index_index];
                    index -= _mesh.vertex_offset;
                    index += vertex_offset;
                    indices.push_back(index);
                }
            }
            render_call.vbo = ref<vertex_buffer>::create(vertices);
            render_call.ibo = ref<index_buffer>::create(indices);
        }
    }
}