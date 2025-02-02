#pragma once

#include <flecs/flecs.h>
#include <string>
#include <unordered_map>
#include "Cam.h"
#include "core/UUID.h"
#include "render/Mesh.h"
#include "scene/Entity.h"

class Entity;

class SceneRenderer;

struct LightInfo {
  glm::vec3 LightDirection;
  glm::vec3 LightPos;
};

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
  glm::mat4 EditTransform(glm::mat4& matrix);

  void OnMouseMove(double xPos, double yPos);
  void ScanKeyPress();

  LightInfo SceneLightInfo;
  std::unique_ptr<PlayerCamera> m_SceneCamera;

 private:
  std::unordered_map<UUID, Entity> m_EntityMap;
  flecs::world m_World;
  std::string m_Name;

  friend class Entity;
};
