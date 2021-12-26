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
#include <entt/entt.hpp>
namespace vkrollercoaster {
    class scene;
    class entity {
    public:
        entity() {
            this->m_scene = nullptr;
            this->reset();
        }
        entity(entt::entity id, scene* scene_) {
            this->m_scene = scene_;
            this->m_id = id;
            if (this->m_scene) {
                this->add_to_entity_set();
            }
        }
        ~entity() {
            if (this->m_scene) {
                this->remove_from_entity_set();
            }
        }
        entity(const entity& other) {
            this->m_scene = other.m_scene;
            this->m_id = other.m_id;
            if (this->m_scene) {
                this->add_to_entity_set();
            }
        }
        entity& operator=(const entity& other) {
            this->m_scene = other.m_scene;
            this->m_id = other.m_id;
            if (this->m_scene) {
                this->add_to_entity_set();
            }
            return *this;
        }

        void reset() {
            if (this->m_scene) {
                this->remove_from_entity_set();
            }
            this->m_id = entt::null;
            this->m_scene = nullptr;
        }

        template <typename T, typename... Args> T& add_component(Args&&... args);
        template <typename T> T& get_component() const;
        template <typename T> bool has_component() const;
        template <typename T> void remove_component();
        operator bool() const { return this->m_id != entt::null && this->m_scene != nullptr; }

        bool operator==(const entity& other) const {
            return this->m_id == other.m_id && this->m_scene == other.m_scene;
        }
        bool operator!=(const entity& other) const { return !(*this == other); }

    private:
        void add_to_entity_set();
        void remove_from_entity_set();

        scene* m_scene;
        entt::entity m_id;
        friend class scene;
        template <typename T> friend struct ::std::hash;
    };
    class scene_serializer;
    class scene : public ref_counted {
    public:
        ~scene() { this->reset(); }

        void reset();
        void update();
        void for_each(std::function<void(entity)> callback);
        entity create(const std::string& tag = "Entity");
        void reevaluate_first_track_node();

        std::vector<entity> find_tag(const std::string& tag);
        entity find_main_camera();
        template <typename... Components> std::vector<entity> view() {
            std::vector<entity> entities;
            auto view = this->m_registry.view<Components...>();
            for (entt::entity id : view) {
                entities.push_back(entity(id, this));
            }
            return entities;
        }

        entity get_first_track_node() { return this->m_first_track_node; }

    private:
        template <typename T> void on_component_added(entity& ent, T& component);
        template <typename T> void on_component_removed(entity ent);
        entt::registry m_registry;
        entity m_first_track_node;
        std::set<entity*> m_entities;
        friend class entity;
        friend class scene_serializer;
    };
    template <typename T, typename... Args> inline T& entity::add_component(Args&&... args) {
        if (this->has_component<T>()) {
            throw std::runtime_error(
                "this entity already has an instance of the specified compoonent type!");
        }
        T& component =
            this->m_scene->m_registry.emplace<T>(this->m_id, std::forward<Args>(args)...);
        this->m_scene->on_component_added(*this, component);
        return component;
    }
    template <typename T> T& entity::get_component() const {
        if (!this->has_component<T>()) {
            throw std::runtime_error(
                "this entity does not have an instance of the specified component type!");
        }
        return this->m_scene->m_registry.get<T>(this->m_id);
    }
    template <typename T> bool entity::has_component() const {
        return this->m_scene->m_registry.all_of<T>(this->m_id);
    }
    template <typename T> void entity::remove_component() {
        if (!this->has_component<T>()) {
            throw std::runtime_error(
                "this entity does not have an instance of the specified component type!");
        }
        this->m_scene->m_registry.remove<T>(this->m_id);
        this->m_scene->on_component_removed<T>(*this);
    }
    template <typename T> void scene::on_component_added(entity& ent, T& component) {
        // no behavior
    }
    template <typename T> void scene::on_component_removed(entity ent) {
        // no behavior
    }
} // namespace vkrollercoaster
namespace std {
    template <> struct hash<vkrollercoaster::entity> {
        size_t operator()(const vkrollercoaster::entity& entity) const {
            return (std::hash<vkrollercoaster::scene*>()(entity.m_scene) << 1) ^
                   std::hash<uint32_t>()((uint32_t)entity.m_id);
        }
    };
} // namespace std