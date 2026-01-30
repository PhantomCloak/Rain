#include "Scene.h"
#include "Application.h"
#include "Components.h"
#include "Entity.h"
#include "SceneRenderer.h"
#include "animation/OzzAnimator.h"
#include "debug/Profiler.h"
#include "glm/gtc/type_ptr.hpp"
#include "imgui.h"
#include "io/cursor.h"
#include "render/ResourceManager.h"

// #include "ImGuizmo.h"

// Jolt includes
namespace Rain
{
  Scene* Scene::Instance = nullptr;

  Scene::Scene(std::string sceneName)
      : m_Name(sceneName) {}

  Entity Scene::CreateEntity(std::string name)
  {
    return CreateChildEntity({}, name);
  }

  float lightX = 0.0;
  float lightY = 90.0;
  float lightZ = 0.0;

  UUID entityIdDir = 0.0;
  UUID Select = 0;
  Entity entityCamera;
  Entity helment;

  void Scene::Init()
  {
    Instance = this;

    auto camera = CreateEntity("MainCamera");
    camera.AddComponent<CameraComponent>();

    // auto bochii = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/assault_rifle_pbr/scene.gltf");
    auto bochii = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/test2/untitled.gltf");
    auto foo = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/box.gltf");

    Entity galata = CreateEntity("bump");
    Entity box = CreateEntity("box");

    galata.Transform().Translation = glm::vec3(0, -0.0, 0);
    galata.Transform().Scale = glm::vec3(0.2f);
    galata.Transform().SetRotationEuler(glm::radians(glm::vec3(-90.0, 0.0, 180.0)));

    box.Transform().Translation = glm::vec3(0, -0.0, 0);
    box.Transform().Scale = glm::vec3(1.0f);

    BuildMeshEntityHierarchy(galata, bochii);
    BuildMeshEntityHierarchy(box, foo);

    Entity light = CreateEntity("DirectionalLight");
    light.GetComponent<TransformComponent>().SetRotationEuler(glm::vec3(glm::radians(lightX), glm::radians(lightY), glm::radians(lightZ)));
    light.AddComponent<DirectionalLightComponent>();
    entityIdDir = light.GetUUID();
  }

  Entity Scene::CreateChildEntity(Entity parent, std::string name)
  {
    Entity entity = Entity(m_World.entity(), this);
    uint32_t entityHandle = entity;

    entity.AddComponent<TransformComponent>();
    if (!name.empty())
    {
      TagComponent& tagComponent = entity.AddComponent<TagComponent>();
      tagComponent.Tag = name;
    }
    entity.AddComponent<RelationshipComponent>();
    IDComponent& idComponent = entity.AddComponent<IDComponent>();
    idComponent.ID = {};

    if (parent)
    {
      entity.SetParent(parent);
    }

    SceneLightInfo.LightPos = glm::vec3(0.0f);
    m_EntityMap[idComponent.ID] = entity;

    return entity;
  }

  Entity Scene::TryGetEntityWithUUID(UUID id) const
  {
    if (const auto iter = m_EntityMap.find(id); iter != m_EntityMap.end())
    {
      return iter->second;
    }
    return {};
  }

  std::pair<glm::vec3, glm::vec3> Scene::CastRay(Entity& cameraEntity, float mx, float my)
  {
    const Camera& camera = cameraEntity.GetComponent<CameraComponent>();
    glm::mat4 cameraViewMatrix = glm::inverse(GetWorldSpaceTransformMatrix(cameraEntity));

    float w = Application::Get()->GetWindowSize().x;
    float h = Application::Get()->GetWindowSize().y;

    float x = (2.0f * mx / w) - 1.0f;
    float y = 1.0f - (2.0f * my / h);

    glm::vec4 clipPos = glm::vec4(x, y, -1.0f, 1.0f);

    glm::mat4 projMatrix = glm::perspectiveFov(glm::radians(55.0f), w, h, 0.10f, 1400.0f);
    glm::mat4 viewMatrix = cameraViewMatrix;

    glm::mat4 invProj = glm::inverse(projMatrix);
    glm::mat4 invView = glm::inverse(viewMatrix);

    glm::vec4 rayEye = invProj * clipPos;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    glm::vec4 rayWorld = invView * rayEye;
    glm::vec3 rayDir = glm::normalize(glm::vec3(rayWorld));

    glm::vec3 rayPos = cameraEntity.Transform().Translation;

    return {rayPos, rayDir};
  }

  void Scene::OnUpdate()
  {
    ScanKeyPress();

    // Update all animators
    float dt = Application::Get()->GetDeltaTime();
    m_World.query<AnimatorComponent>().each([dt](AnimatorComponent& ac)
                                            {
      if (ac.Playing && ac.Animator)
      {
        ac.Animator->Update(dt);
      } });

    m_PhysicsScene->Update(1.0f / 60.0);
    Cursor::Update();

    ImGui::Begin("A");
    ImGui::SliderFloat("Light X", &lightX, -1.0f, 1.0f);
    ImGui::SliderFloat("Light Y", &lightY, -1.0f, 1.0f);
    ImGui::SliderFloat("Light Z", &lightZ, -1.0f, 1.0f);
    ImGui::End();
  }

  glm::mat4 Scene::EditTransform(glm::mat4& matrix)
  {
  }

  Entity Scene::GetMainCameraEntity()
  {
    if (m_World != nullptr)
    {
      flecs::entity e = m_World.query_builder<CameraComponent>().build().first();
      if (e)
      {
        return Entity(e, this);
      }
    }

    return {};
  }

  void Scene::OnRender(Ref<SceneRenderer> renderer)
  {
    RN_PROFILE_FUNC;
    renderer->SetScene(this);

    auto far = 400.0f;
    Entity cameraEntity = GetMainCameraEntity();
    Camera& camera = cameraEntity.GetComponent<CameraComponent>();
    glm::mat4 cameraViewMatrix = glm::inverse(GetWorldSpaceTransformMatrix(cameraEntity));

    static float w = Application::Get()->GetWindowSize().x;
    static float h = Application::Get()->GetWindowSize().y;
    camera.SetPerspectiveProjectionMatrix(glm::radians(55.0f), w, h, 0.10f, far);
    renderer->BeginScene({cameraViewMatrix, camera.GetProjectionMatrix(), 10.0f, far});

    static float lightX = 0.637f;
    static float lightY = 0.514f;
    static float lightZ = 0.0f;

    static flecs::query<TransformComponent, MeshComponent> drawNodeQuery = m_World.query<TransformComponent, MeshComponent>();
    drawNodeQuery.each([&](flecs::entity entity, TransformComponent& transform, MeshComponent& meshComponent)
                       {
      Entity e = Entity(entity, this);
      Ref<MeshSource> meshSource = Rain::ResourceManager::GetMeshSource(meshComponent.MeshSourceId);
      glm::mat4 entityTransform = GetWorldSpaceTransformMatrix(e);

      // Check if this entity has an animator component
      Ref<OzzAnimator> animator = nullptr;
      if (entity.has<AnimatorComponent>())
      {
      auto animComp = entity.get<AnimatorComponent>();
      animator = animComp.Animator;
      }

      renderer->SubmitMesh(meshSource, meshComponent.SubMeshId, meshComponent.Materials, entityTransform, animator); });

    SceneLightInfo.LightDirection = glm::normalize(glm::vec3(lightX, lightY, lightZ));
    SceneLightInfo.LightPos = glm::vec3(0.0f);

    renderer->EndScene();
  }

  void Scene::BuildMeshEntityHierarchy(Entity parent, Ref<MeshSource> mesh)
  {
    const std::vector<Ref<MeshNode>> nodes = mesh->GetNodes();

    // Create animator if mesh has ozz animations
    Ref<OzzAnimator> animator = nullptr;
    if (mesh->HasOzzSkeleton() && mesh->GetOzzAnimationCount() > 0)
    {
      animator = CreateRef<OzzAnimator>();
      if (animator->Initialize(mesh->GetOzzSkeleton()))
      {
        animator->SetAnimation(mesh->GetOzzAnimation(0));
        animator->SetPlaying(true);
        animator->SetLooping(true);
        RN_LOG("Created OzzAnimator for mesh with {} animations", mesh->GetOzzAnimationCount());
      }
      else
      {
        RN_LOG_ERR("Failed to initialize OzzAnimator");
        animator = nullptr;
      }
    }

    for (const Ref<MeshNode> node : mesh->GetNodes())
    {
      if (node->IsRoot() && mesh->m_SubMeshes[node->SubMeshId].VertexCount == 0)
      {
        continue;
      }
      Entity nodeEntity = CreateChildEntity(parent, node->Name);
      uint32_t materialId = mesh->m_SubMeshes[node->SubMeshId].MaterialIndex;

      nodeEntity.AddComponent<MeshComponent>(mesh->Id, (uint32_t)node->SubMeshId, materialId);
      nodeEntity.AddComponent<TransformComponent>();

      nodeEntity.Transform().SetTransform(node->LocalTransform);

      // Attach animator to all mesh entities so animation works for all submeshes
      if (animator)
      {
        AnimatorComponent& animComp = nodeEntity.AddComponent<AnimatorComponent>();
        animComp.Animator = animator;
        animComp.Playing = true;
      }
    }
  }

  glm::mat4 Scene::GetWorldSpaceTransformMatrix(Entity entity)
  {
    glm::mat4 transform = glm::identity<glm::mat4>();

    Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());
    if (parent)
    {
      transform = GetWorldSpaceTransformMatrix(parent);
    }

    return transform * entity.GetComponent<TransformComponent>().GetTransform();
  }

  void Scene::ConvertToLocalSpace(Entity entity)
  {
    Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());

    if (!parent)
    {
      return;
    }

    TransformComponent& transform = entity.Transform();
    glm::mat4 parentTransform = GetWorldSpaceTransformMatrix(parent);
    glm::mat4 localTransform = glm::inverse(parentTransform) * transform.GetTransform();
    transform.SetTransform(localTransform);
  }

  TransformComponent Scene::GetWorldSpaceTransform(Entity entity)
  {
    glm::mat4 transform = GetWorldSpaceTransformMatrix(entity);
    TransformComponent transformComponent;
    transformComponent.SetTransform(transform);
    return transformComponent;
  }

  void Scene::ScanKeyPress()
  {
  }
}  // namespace Rain
