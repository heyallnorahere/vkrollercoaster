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
namespace vkrollercoaster {
    class scene;
    class entity {
    public:
        entity() {
            this->m_id = entt::null;
            this->m_scene = nullptr;
        }
        entity(entt::entity id, scene* scene_) {
            this->m_scene = scene_;
            this->m_id = id;
        }
        template<typename T, typename... Args> T& add_component(Args&&... args);
        template<typename T> T& get_component();
        template<typename T> bool has_component();
        template<typename T> void remove_component();
        operator bool() { return this->m_id != entt::null && this->m_scene != nullptr; }
    private:
        scene* m_scene;
        entt::entity m_id;
    };
    class scene {
    public:
        entity create();
        entity create(const std::string& tag);
        std::vector<entity> find_tag(const std::string& tag);
        template<typename... Components> std::vector<entity> view() {
            std::vector<entity> entities;
            auto view = this->m_registry.view<Components...>();
            for (entt::entity id : view) {
                entities.push_back(entity(id, this));
            }
            return entities;
        }
    private:
        template<typename T> void on_component_added(entity& ent, T& component);
        entt::registry m_registry;
        friend class entity;
    };
    template<typename T, typename... Args> inline T& entity::add_component(Args&&... args) {
        if (this->has_component<T>()) {
            throw std::runtime_error("this entity already has an instance of the specified compoonent type!");
        }
        T& component = this->m_scene->m_registry.emplace<T>(this->m_id, std::forward<Args>(args)...);
        this->m_scene->on_component_added(*this, component);
        return component;
    }
    template<typename T> T& entity::get_component() {
        if (!this->has_component<T>()) {
            throw std::runtime_error("this entity does not have an instance of the specified component type!");
        }
        return this->m_scene->m_registry.get<T>(this->m_id);
    }
    template<typename T> bool entity::has_component() {
        return this->m_scene->m_registry.all_of<T>(this->m_id);
    }
    template<typename T> void entity::remove_component() {
        if (!this->has_component<T>()) {
            throw std::runtime_error("this entity does not have an instance of the specified component type!");
        }
        this->m_scene->m_registry.remove<T>(this->m_scene);
    }
    template<typename T> void scene::on_component_added(entity& ent, T& component) {
        // no behavior
    }
}