#pragma once

#include <flecs.h>
#include <memory>
#include <string>
#include "Cam.h"
#include "core/UUID.h"
#include "physics/PhysicsScene.h"
#include "render/Mesh.h"
#include "scene/Entity.h"

namespace Rain {

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
    TransformComponent GetWorldSpaceTransform(Entity entity);
    glm::mat4 EditTransform(glm::mat4& matrix);
    void ConvertToLocalSpace(Entity entity);

    std::pair<glm::vec3, glm::vec3> CastRay(const PlayerCamera& camera, float mx, float my);
    void OnMouseMove(double xPos, double yPos);
    void ScanKeyPress();

    LightInfo SceneLightInfo;
    std::unique_ptr<PlayerCamera> m_SceneCamera;
    static Scene* Instance;

   private:
    std::unordered_map<UUID, Entity> m_EntityMap;
    flecs::world m_World;
    std::string m_Name;
    Ref<PhysicsScene> m_PhysicsScene;
    friend class Entity;
  };
}  // namespace Rain
