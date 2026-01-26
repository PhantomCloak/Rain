#include "Scene.h"
#include "Application.h"
#include "Components.h"
#include "Entity.h"
#include "SceneRenderer.h"
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

  // Orbit camera state
  static glm::vec3 orbitTarget = glm::vec3(0.0f);  // Focus point (center of model)
  static float orbitDistance = 30.0f;              // Distance from target
  static float orbitTheta = 0.0f;                  // Horizontal angle (radians)
  static float orbitPhi = glm::radians(30.0f);     // Vertical angle (radians), 0 = horizontal
  static glm::vec2 lastMousePos = glm::vec2(0.0f);
  static bool isDragging = false;

  // Updates camera position based on orbit parameters
  void UpdateOrbitCamera(Entity& camera)
  {
    // Spherical to Cartesian conversion
    // x = r * cos(phi) * sin(theta)
    // y = r * sin(phi)
    // z = r * cos(phi) * cos(theta)
    float x = orbitDistance * cos(orbitPhi) * sin(orbitTheta);
    float y = orbitDistance * sin(orbitPhi);
    float z = orbitDistance * cos(orbitPhi) * cos(orbitTheta);

    glm::vec3 cameraPos = orbitTarget + glm::vec3(x, y, z);
    camera.Transform().Translation = cameraPos;

    // Look at target using glm::lookAt to get the view matrix, then extract rotation
    glm::mat4 viewMatrix = glm::lookAt(cameraPos, orbitTarget, glm::vec3(0, 1, 0));
    glm::mat4 rotationMatrix = glm::inverse(viewMatrix);
    camera.Transform().Rotation = glm::quat_cast(rotationMatrix);
  }

  void Scene::Init()
  {
    Instance = this;

    auto camera = CreateEntity("MainCamera");
    camera.AddComponent<CameraComponent>();

    // auto bochii = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/assault_rifle_pbr/scene.gltf");
    auto bochii = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/test/untitled.gltf");
    Entity galata = CreateEntity("bump");

    galata.Transform().Translation = glm::vec3(0, -0.45f, 0);
    galata.Transform().Scale = glm::vec3(1);
    // galata.Transform().SetRotationEuler(glm::radians(glm::vec3(90.0, 0.0, 180.0)));

    // Set orbit target to match the model position
    orbitTarget = galata.Transform().Translation;

    BuildMeshEntityHierarchy(galata, bochii);

    // Initialize camera position from orbit parameters
    UpdateOrbitCamera(camera);

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

    m_PhysicsScene->Update(1.0f / 60.0);
    Cursor::Update();
  }

  glm::mat4 Scene::EditTransform(glm::mat4& matrix)
  {
  }

  UUID RenderNode(Entity e, Scene* scene)
  {
    // TagComponent tag = e.GetComponent<TagComponent>();
    // static UUID selectedNode = -1;
    // UUID currentNode = e.GetComponent<IDComponent>().ID;
    // int childCount = 0;
    // RelationshipComponent r = {};
    // bool isRempty = true;

    // if (e.HasComponent<RelationshipComponent>()) {
    //   r = e.GetComponent<RelationshipComponent>();
    //   isRempty = true;
    //   childCount = r.Children.size();
    // }

    // ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    // if (childCount == 0) {
    //   flags |= ImGuiTreeNodeFlags_Leaf;
    // }
    // if (selectedNode == currentNode) {
    //   flags |= ImGuiTreeNodeFlags_Selected;
    // }

    // if (ImGui::TreeNodeEx(tag.Tag.c_str(), flags)) {
    //   if (ImGui::IsItemClicked()) {
    //     RN_LOG("Name: {} ID: {} C: {}", tag.Tag, (uint64_t)currentNode, (uint64_t)selectedNode);
    //     selectedNode = currentNode;
    //   }

    //  // if (r != {}) {
    //  if (!isRempty) {
    //    if (r.Children.size() > 0) {
    //      ImGui::Indent(0.5f);
    //    }

    //    for (auto child : r.Children) {
    //      Entity childEntity = scene->TryGetEntityWithUUID(child);
    //      RenderNode(childEntity, scene);
    //    }
    //  }

    //  ImGui::TreePop();
    //}

    // return selectedNode;
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

    ImGui::Begin("A");
    ImGui::SliderFloat("Light X", &lightX, -1.0f, 1.0f);
    ImGui::SliderFloat("Light Y", &lightY, -1.0f, 1.0f);
    ImGui::SliderFloat("Light Z", &lightZ, -1.0f, 1.0f);
    ImGui::End();

    static flecs::query<TransformComponent, MeshComponent> drawNodeQuery = m_World.query<TransformComponent, MeshComponent>();
    drawNodeQuery.each([&](flecs::entity entity, TransformComponent& transform, MeshComponent& meshComponent)
                       {
      Entity e = Entity(entity, this);
      Ref<MeshSource> meshSource = Rain::ResourceManager::GetMeshSource(meshComponent.MeshSourceId);
      glm::mat4 entityTransform = GetWorldSpaceTransformMatrix(e);
      renderer->SubmitMesh(meshSource, meshComponent.SubMeshId, meshComponent.Materials, entityTransform); });

    SceneLightInfo.LightDirection = glm::normalize(glm::vec3(lightX, lightY, lightZ));
    SceneLightInfo.LightPos = glm::vec3(0.0f);

    renderer->EndScene();
  }

  void Scene::BuildMeshEntityHierarchy(Entity parent, Ref<MeshSource> mesh)
  {
    const std::vector<Ref<MeshNode>> nodes = mesh->GetNodes();

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
    Entity camera = GetMainCameraEntity();
    if (!camera)
    {
      return;
    }

    // Scroll wheel zoom only
    float scroll = Cursor::GetScrollDelta();
    if (scroll != 0.0f)
    {
      orbitDistance -= scroll * 1.0f;
      orbitDistance = glm::clamp(orbitDistance, 1.0f, 100.0f);
      Cursor::ResetScrollDelta();
      UpdateOrbitCamera(camera);
    }
  }

  void Scene::OnMouseMove(double xPos, double yPos)
  {
    if (ImGui::GetIO().WantCaptureMouse)
    {
      return;
    }

    glm::vec2 currentMousePos = glm::vec2(xPos, yPos);

    Entity camera = GetMainCameraEntity();
    if (!camera)
    {
      return;
    }

    // Left mouse button held - orbit rotation
    if (Cursor::HasLeftCursorClicked())
    {
      if (!isDragging)
      {
        isDragging = true;
        lastMousePos = currentMousePos;
        return;
      }

      glm::vec2 delta = currentMousePos - lastMousePos;
      lastMousePos = currentMousePos;

      float sensitivity = 0.005f;

      // Horizontal drag rotates around Y axis (theta)
      orbitTheta += delta.x * sensitivity;

      // Vertical drag changes elevation (phi)
      orbitPhi += delta.y * sensitivity;

      // Clamp phi to avoid flipping (keep between -89 and 89 degrees)
      orbitPhi = glm::clamp(orbitPhi, glm::radians(-89.0f), glm::radians(89.0f));

      UpdateOrbitCamera(camera);
    }
    else
    {
      isDragging = false;
    }
  }
}  // namespace Rain
