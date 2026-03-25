#pragma once

#include <flecs.h>
#include <memory>
#include <string>
#include "core/UUID.h"
#include "physics/PhysicsScene.h"
#include "render/Camera.h"
#include "render/Mesh.h"
#include "scene/Entity.h"

namespace WebEngine
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
    Entity CreateChildEntity(const Entity& parent, const std::string name);

    virtual void Init();
    virtual void Cleanup();

    void OnUpdate();
    void OnRender(Ref<SceneRenderer> renderer, const glm::mat4& editorViewMatrix = glm::mat4(0.0f));

    void BuildMeshEntityHierarchy(const Entity& parent, const Ref<MeshSource>& mesh);
    Entity TryGetEntityWithUUID(const UUID& id) const;
    glm::mat4 GetWorldSpaceTransformMatrix(const Entity& entity);
    TransformComponent GetWorldSpaceTransform(const Entity& entity);
    glm::mat4 EditTransform(glm::mat4& matrix);
    void ConvertToLocalSpace(Entity entity);
    Entity GetMainCameraEntity();

    std::pair<glm::vec3, glm::vec3> CastRay(Entity& cameraEntity, float mx, float my);
    virtual void ScanKeyPress();

    glm::vec3 EditorCameraPosition = glm::vec3(0.0f);
    glm::vec3 EditorCameraForward = glm::vec3(0.0f, 0.0f, -1.0f);

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
    std::unique_ptr<PhysicsScene> m_PhysicsScene;
    friend class Entity;
    friend class DemoScenePhysicCollisions;
  };

}  // namespace WebEngine
