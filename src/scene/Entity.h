#pragma once

#include <vector>
#include "Components.h"
#include "core/UUID.h"
#include "flecs/flecs.h"

#define NoName "NoName"

class Scene;

class Entity {
 public:
  Entity() = default;
  Entity(flecs::entity id, Scene* scene)
      : m_Entity(id), m_Scene(scene) {}
  ~Entity() {}

  template <typename T, typename... Args>
  void AddComponent(Args&&... args) {
    // RN_ASSERT(!HasComponent<T>(), "Entity: Already has component!.");
    m_Entity.set<T>({std::forward<Args>(args)...});
  }

  template <typename T, typename... Args>
  T* AddComponent() {
    m_Entity.add<T>();
    return GetComponent<T>();
  }

  template <typename T>
  T* GetComponent() {
    auto a = m_Entity.get_ref<T>();
    return m_Entity.get_mut<T>();
  }

  template <typename T>
  const T* GetComponent() const {
    return m_Entity.get<T>();
  }

  template <typename... T>
  bool HasComponent() {
    return m_Entity.has<T...>();
  }

  template <typename... T>
  bool HasComponent() const {
    return m_Entity.has<T...>();
  }

  Entity GetParent() const;
  UUID GetChild(int index) { return Children()[index]; }

  bool RemoveChild(Entity child) {
    UUID childId = child.GetUUID();
    std::vector<UUID>& children = Children();
    auto it = std::find(children.begin(), children.end(), childId);
    if (it != children.end()) {
      children.erase(it);
      return true;
    }

    return false;
  }

  void SetParent(Entity parent) {
    Entity currentParent = GetParent();

    if (currentParent == parent) {
      return;
    }

    if (currentParent) {
      currentParent.RemoveChild(*this);
    }

    SetParentUUID(parent.GetUUID());

    if (parent) {
      auto& parentChildren = parent.Children();
      UUID uuid = GetUUID();
      if (std::find(parentChildren.begin(), parentChildren.end(), uuid) == parentChildren.end()) {
        parentChildren.emplace_back(GetUUID());
      }
    }
  }

  const std::string Name() const { return HasComponent<TagComponent>() ? GetComponent<TagComponent>()->Tag : NoName; }
  void SetParentUUID(UUID parent) { GetComponent<RelationshipComponent>()->ParentHandle = parent; }
  std::vector<UUID>& Children() { return GetComponent<RelationshipComponent>()->Children; }
  UUID GetParentUUID() const { return GetComponent<RelationshipComponent>()->ParentHandle; }
  UUID GetUUID() const { return GetComponent<IDComponent>()->ID; }

  TransformComponent* Transform() { return GetComponent<TransformComponent>(); }
  const glm::mat4 Transform() const { return GetComponent<TransformComponent>()->GetTransform(); }

  operator bool() const;

  bool operator==(const Entity& other) const {
    return m_Entity == other.m_Entity && m_Scene == other.m_Scene;
  }

  bool operator!=(const Entity& other) const {
    return !(*this == other);
  }

  operator uint32_t() const { return (uint32_t)m_Entity; }

 private:
  flecs::entity m_Entity;
  Scene* m_Scene;

  friend class Scene;
};
