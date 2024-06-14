#pragma once

#include <flecs/flecs.h>
#include <map>
#include <string>
#include "Cam.h"
#include "core/UUID.h"
#include "render/Mesh.h"

class Entity;

class SceneRenderer;

class Scene {
 public:
  Scene(std::string sceneName = "Untitled Scene");

  Entity CreateEntity(std::string name);
  Entity CreateChildEntity(Entity parent, std::string name);

  void Init();

  void OnUpdate();
  void OnRender(Ref<SceneRenderer> renderer);

  void BuildMeshEntityHierarchy(Entity parent, Ref<MeshSource> mesh);
  Entity TryGetEntityWithUUID(UUID id) const;
  glm::mat4 GetWorldSpaceTransformMatrix(Entity entity);

	PlayerCamera* m_SceneCamera;

 private:
  std::unordered_map<UUID, Entity> m_EntityMap;

  flecs::world m_World;
  std::string m_Name;

  friend class Entity;
};
