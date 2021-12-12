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
    template <glm::length_t L, typename T>
    static glm::vec<L, float> convert(const T& assimp_vector) {
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
        virtual void write(const char* message) override { spdlog::warn("assimp: {0}", message); }
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
    model_source::model_source(const fs::path& path) {
        initialize_logger();
        this->m_path = path;
        if (!this->m_path.is_absolute()) {
            this->m_path = fs::absolute(this->m_path);
        }
        this->reload();
    }
    static constexpr uint32_t import_flags = aiProcess_Triangulate | aiProcess_GenNormals |
                                             aiProcess_GenUVCoords | aiProcess_OptimizeMeshes |
                                             aiProcess_JoinIdenticalVertices |
                                             aiProcess_ValidateDataStructure | aiProcess_FlipUVs;
    void model_source::reload() {
        this->m_importer = std::make_unique<Assimp::Importer>();
        this->m_vertices.clear();
        this->m_indices.clear();
        this->m_meshes.clear();
        this->m_materials.clear();
        this->m_scene = this->m_importer->ReadFile(this->m_path.string(), import_flags);
        if (!this->m_scene || this->m_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
            !this->m_scene->mRootNode) {
            throw std::runtime_error("could not load model: " +
                                     std::string(this->m_importer->GetErrorString()));
        }
        std::vector<ref<material>> materials;
        this->process_materials();
        this->process_node(this->m_scene->mRootNode);

        for (model* _model : this->m_created_models) {
            _model->acquire_mesh_data();
            _model->assemble_index_map();
        }
    }
    void model_source::process_node(aiNode* node) {
        for (size_t i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh_ = this->m_scene->mMeshes[node->mMeshes[i]];
            this->process_mesh(mesh_, node);
        }
        for (size_t i = 0; i < node->mNumChildren; i++) {
            aiNode* child = node->mChildren[i];
            this->process_node(child);
        }
    }
    void model_source::process_mesh(aiMesh* mesh_, aiNode* node) {
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
        util::append_vector(this->m_vertices, vertices);
        util::append_vector(this->m_indices, indices);
    }
    void model_source::process_materials() {
        // todo: change when skinning
        const std::string shader_name = "default_static";
        ref<shader> material_shader = shader_library::get(shader_name);

        for (size_t i = 0; i < this->m_scene->mNumMaterials; i++) {
            auto _material = ref<material>::create(material_shader);
            aiMaterial* ai_material = this->m_scene->mMaterials[i];
            aiString ai_string;

            // name
            if (ai_material->Get(AI_MATKEY_NAME, ai_string) == aiReturn_SUCCESS) {
                _material->set_name(ai_string.C_Str());
            }

            // albedo map
            if (ai_material->GetTexture(aiTextureType_DIFFUSE, 0, &ai_string) == aiReturn_SUCCESS) {
                auto path = this->get_resource_path(ai_string);
                auto img = image::from_file(path);
                if (img) {
                    auto tex = ref<texture>::create(img);
                    _material->set_texture("albedo_texture", tex);
                }
            }

            // specular map
            if (ai_material->GetTexture(aiTextureType_SPECULAR, 0, &ai_string) ==
                aiReturn_SUCCESS) {
                auto path = this->get_resource_path(ai_string);
                auto img = image::from_file(path);
                if (img) {
                    auto tex = ref<texture>::create(img);
                    _material->set_texture("specular_texture", tex);
                }
            }

            // shininess and opacity
            float shininess, opacity;
            if (ai_material->Get(AI_MATKEY_SHININESS, shininess) != aiReturn_SUCCESS) {
                shininess = 80.f;
            }
            if (ai_material->Get(AI_MATKEY_OPACITY, opacity) != aiReturn_SUCCESS) {
                opacity = 1.f;
            }
            _material->set_data("shininess", shininess);
            _material->set_data("opacity", opacity);

            // albedo and specular colors
            glm::vec3 albedo_color = glm::vec3(1.f);
            glm::vec3 specular_color = glm::vec3(1.f);
            aiColor3D ai_color;
            if (ai_material->Get(AI_MATKEY_COLOR_DIFFUSE, ai_color) == aiReturn_SUCCESS) {
                albedo_color = convert<3>(ai_color);
            }
            if (ai_material->Get(AI_MATKEY_COLOR_SPECULAR, ai_color) == aiReturn_SUCCESS) {
                specular_color = convert<3>(ai_color);
            }
            _material->set_data("albedo_color", albedo_color);
            _material->set_data("specular_color", specular_color);

            this->m_materials.push_back(_material);
        }
    }
    fs::path model_source::get_resource_path(const aiString& ai_path) {
        fs::path path = ai_path.C_Str();
        if (path.is_relative()) {
            path = this->m_path.parent_path() / path;
        }
        return path;
    }

    model::model(ref<model_source> source) {
        this->m_source = source;
        this->m_source->m_created_models.insert(this);

        this->set_input_layout();
        this->acquire_mesh_data();
        this->assemble_index_map();
    }

    model::model(const std::vector<ref<material>>& materials, const std::vector<mesh>& meshes,
                 const std::vector<vertex>& vertices, const std::vector<uint32_t>& indices) {
        this->set_input_layout();

        this->m_materials = materials;
        this->m_meshes = meshes;
        this->m_vertices = vertices;
        this->m_indices = indices;

        this->assemble_index_map();
    }

    model::~model() {
        if (this->m_source) {
            this->m_source->m_created_models.erase(this);
        }
    }

    void model::set_input_layout() {
        // todo: use different vertex structure if animated
        this->m_input_layout.stride = sizeof(vertex);
        this->m_input_layout.attributes = {
            { vertex_attribute_type::VEC3, offsetof(vertex, position) },
            { vertex_attribute_type::VEC3, offsetof(vertex, normal) },
            { vertex_attribute_type::VEC2, offsetof(vertex, uv) },
        };
    }

    void model::acquire_mesh_data() {
        // can't take data from nullptr!
        if (!this->m_source) {
            return;
        }

        this->m_vertices = this->m_source->m_vertices;
        this->m_indices = this->m_source->m_indices;
        this->m_materials = this->m_source->m_materials;

        this->m_meshes.clear();
        for (const auto& _mesh : this->m_source->m_meshes) {
            mesh to_insert;

            to_insert.index_offset = _mesh.index_offset;
            to_insert.index_count = _mesh.index_count;
            to_insert.material_index = _mesh.material_index;

            this->m_meshes.push_back(to_insert);
        }
    }

    void model::assemble_index_map() {
        // we put together a map of material indices to vertex indices so we don't have to in
        // renderer::render, thus decreasing render times
        this->m_index_map.clear();
        for (const auto& _mesh : this->m_meshes) {
            auto begin = this->m_indices.begin() + _mesh.index_offset;
            auto end = begin + _mesh.index_count;

            auto& indices = this->m_index_map[_mesh.material_index];
            indices.insert(indices.begin(), begin, end);
        }
    }
} // namespace vkrollercoaster