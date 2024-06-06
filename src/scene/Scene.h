#pragma once
#include <flecs/flecs.h>
#include <string>
#include "render/Model.h"
#include "scene/Components.h"

#define NoName "NoName"
class Entity;

struct MeshKey {
  UUID MeshHandle;
  UUID MaterialHandle;
  uint32_t SubmeshIndex;
};

struct DrawCommand {
  Ref<MeshSource> Mesh;
  uint32_t SubmeshIndex;
  uint32_t MaterialIndex;

  uint32_t InstanceCount = 0;
  uint32_t InstanceOffset = 0;
  bool IsRigged = false;
};

class Scene {
 public:
  Scene(std::string sceneName = "Untitled Scene");

  Entity CreateEntity(std::string name);
  Entity CreateChildEntity(Entity parent, std::string name);

  void OnRender();
  void BuildMeshEntityHierarchy(Entity parent, Ref<MeshSource> mesh);
  Entity TryGetEntityWithUUID(UUID id) const;
  glm::mat4 GetWorldSpaceTransformMatrix(Entity entity);

  void SubmitMesh(Ref<MeshSource> meshSource, uint32_t submeshIndex, uint32_t materialIndex, glm::mat4& transform);

 private:
  std::unordered_map<UUID, Entity> m_EntityMap;
  std::map<MeshKey, DrawCommand> m_DrawList;
	std::map<MeshKey, std::vector<SceneUniform>> m_MeshTransformMap;
  flecs::world m_World;
  std::string m_Name;

  friend class Entity;
};

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
    // flecs::ref
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
    UUID cuid = GetUUID();
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
  operator bool() const;

  bool operator==(const Entity& other) const {
    return m_Entity == other.m_Entity && m_Scene == other.m_Scene;
  }

  bool operator!=(const Entity& other) const {
    return !(*this == other);
  }

 private:
  flecs::entity m_Entity;
  Scene* m_Scene;

  friend class Scene;
};
