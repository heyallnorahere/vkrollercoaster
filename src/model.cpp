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
        Assimp::DefaultLogger::get()->attachStream(new error_logstream, Assimp::Logger::Err);
        Assimp::DefaultLogger::get()->attachStream(new warning_logstream, Assimp::Logger::Warn);
    }
    model::model(const fs::path& path) {
        initialize_logger();
        this->m_path = path;
        this->reload();
    }
    static constexpr uint32_t import_flags =
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_GenUVCoords |
        aiProcess_OptimizeMeshes |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ValidateDataStructure;
    void model::reload() {
        this->m_importer = std::make_unique<Assimp::Importer>();
        this->m_vertices.clear();
        this->m_indices.clear();
        this->m_meshes.clear();
        this->m_scene = this->m_importer->ReadFile(this->m_path.string(), import_flags);
        if (!this->m_scene || this->m_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !this->m_scene->mRootNode) {
            throw std::runtime_error("could not load model: " + std::string(this->m_importer->GetErrorString()));
        }
        this->process_node(this->m_scene->mRootNode);
        // todo: materials
    }
    void model::process_node(aiNode* node) {
        for (size_t i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh_ = this->m_scene->mMeshes[node->mMeshes[i]];
            this->process_mesh(mesh_, node);
        }
        for (size_t i = 0; i < node->mNumChildren; i++) {
            aiNode* child = node->mChildren[i];
            this->process_node(child);
        }
    }
    void model::process_mesh(aiMesh* mesh_, aiNode* node) {
        std::vector<vertex> vertices;
        std::vector<uint32_t> indices;
        for (size_t i = 0; i < mesh_->mNumVertices; i++) {
            vertex v;
            v.position = convert<3>(mesh_->mVertices[i]) * glm::vec3(1.f, -1.f, 1.f);
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
        mesh& mesh_data = this->m_meshes.emplace_back();
        mesh_data.vertex_offset = this->m_vertices.size();
        mesh_data.index_offset = this->m_indices.size();
        mesh_data.assimp_mesh = mesh_;
        mesh_data.node = node;
        mesh_data.vertex_count = vertices.size();
        mesh_data.index_count = indices.size();
        util::append_vector(this->m_vertices, vertices);
        util::append_vector(this->m_indices, indices);
    }
}