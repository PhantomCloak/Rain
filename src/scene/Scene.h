#pragma once

#include <flecs.h>
#include <memory>
#include <string>
#include "core/UUID.h"
#include "physics/PhysicsScene.h"
#include "render/Camera.h"
#include "render/Mesh.h"
#include "scene/Entity.h"

namespace Rain
{

  class SceneRenderer;

  struct LightInfo
  {
    glm::vec3 LightDirection;
    glm::vec3 LightPos;
  };

  class Scene
  {
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
    Entity GetMainCameraEntity();

    std::pair<glm::vec3, glm::vec3> CastRay(Entity& cameraEntity, float mx, float my);
    void ScanKeyPress();

    LightInfo SceneLightInfo;
    std::unique_ptr<Camera> m_SceneCamera;
    static Scene* Instance;

    template <typename... Components>
    std::vector<Entity> GetAllEntitiesWithComponent()  // Debug purposes
    {
      std::vector<Entity> entities;
      m_World.query<Components...>().each([this, &entities](flecs::entity e, Components&...)
                                          { entities.emplace_back(e, this); });
      return entities;
    }

   private:
    std::unordered_map<UUID, Entity> m_EntityMap;
    flecs::world m_World;
    std::string m_Name;
    Ref<PhysicsScene> m_PhysicsScene;
    friend class Entity;
  };
}  // namespace Rain
