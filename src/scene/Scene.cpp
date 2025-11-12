#include "Scene.h"
#include "Application.h"
#include "Components.h"
#include "Entity.h"
#include "SceneRenderer.h"
#include "debug/Profiler.h"
#include "glm/gtc/type_ptr.hpp"
// #include "imgui.h"
#include "io/cursor.h"
#include "io/keyboard.h"
#include "physics/Physics.h"
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

  float rotX = 140.0;
  float rotY = 0.0;
  float rotZ = 25.0;

  UUID entityIdDir = 0.0;
  UUID Select = 0;
  Entity entityCamera;
  Entity helment;

  void Scene::Init()
  {
    Instance = this;

    auto camera = CreateEntity("MainCamera");
    camera.Transform().Translation = glm::vec3(0.5f, 10, 0);
    camera.Transform().SetRotationEuler(glm::vec3(-90.0f, 0, 0));
    camera.AddComponent<CameraComponent>();

    auto boxModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/box.gltf");
    auto bochii = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/galata/galata.gltf");

    helment = CreateEntity("Box");
    Entity floorEntity = CreateEntity("Box2");
    Entity galata = CreateEntity("bump");
    Entity tankEntity = CreateEntity("Tank");

    galata.Transform().Translation = glm::vec3(0, 1, 0);
    galata.Transform().Scale = glm::vec3(0.08f);

    floorEntity.Transform().Translation = glm::vec3(0, 0, 0);
    floorEntity.Transform().Scale = glm::vec3(100, 1.0f, 100);

    // floorEntity.AddComponent<BoxColliderComponent>(glm::vec3(0), glm::vec3(100, 1, 100));
    // floorEntity.AddComponent<RigidBodyComponent>();

    BuildMeshEntityHierarchy(floorEntity, boxModel);
    BuildMeshEntityHierarchy(galata, bochii);

    Entity light = CreateEntity("DirectionalLight");
    light.GetComponent<TransformComponent>().SetRotationEuler(glm::vec3(glm::radians(rotX), glm::radians(rotY), glm::radians(rotZ)));
    light.AddComponent<DirectionalLightComponent>();
    entityIdDir = light.GetUUID();

    // Physics::Instance = new Physics();
    // m_PhysicsScene = Physics::Instance->CreateScene(glm::vec3(0.0, -9.8f, 0.0));
    // m_PhysicsScene->CreateBody(floorEntity);
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

    if (Cursor::WasLeftCursorPressed())
    {
      SceneQueryHit hit;
      auto camera = GetMainCameraEntity();
      auto [origin, direction] = CastRay(camera, Cursor::GetCursorPosition().x, Cursor::GetCursorPosition().y);
      RayCastInfo info;
      info.Origin = origin;
      info.Direction = direction;
      info.MaxDistance = 1000;

      if (m_PhysicsScene->CastRay(&info, hit))
      {
        RN_LOG("Success Entity {}", hit.HitEntity);
        Select = hit.HitEntity;
      }
    }

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
    flecs::entity e = m_World.query_builder<CameraComponent>().build().first();
    if (e)
    {
      return Entity(e, this);
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

    static flecs::query<TransformComponent, MeshComponent> drawNodeQuery = m_World.query<TransformComponent, MeshComponent>();
    drawNodeQuery.each([&](flecs::entity entity, TransformComponent& transform, MeshComponent& meshComponent)
                       {
      Entity e = Entity(entity, this);
      Ref<MeshSource> meshSource = Rain::ResourceManager::GetMeshSource(meshComponent.MeshSourceId);
      glm::mat4 entityTransform = GetWorldSpaceTransformMatrix(e);
      renderer->SubmitMesh(meshSource, meshComponent.SubMeshId, meshComponent.Materials, entityTransform); });

    static flecs::query<TransformComponent, DirectionalLightComponent> lightsQuery = m_World.query<TransformComponent, DirectionalLightComponent>();
    lightsQuery.each([&](flecs::entity entity, TransformComponent& transform, DirectionalLightComponent& directionalLight)
                     {
      SceneLightInfo.LightDirection = glm::normalize(glm::mat3(transform.GetTransform()) * glm::vec3(0.0f, 0.0f, -1.0f));
      SceneLightInfo.LightPos = glm::vec3(0.0f); });

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
    float velocity = 0.55f;

    Entity camera = GetMainCameraEntity();
    TransformComponent& transform = camera.Transform();

    glm::mat4 rotationMatrix = glm::mat4_cast(transform.Rotation);
    glm::vec3 front = -glm::vec3(rotationMatrix[2]);  // Forward (negative Z)
    glm::vec3 right = glm::vec3(rotationMatrix[0]);   // Right (positive X)
    glm::vec3 up = glm::vec3(rotationMatrix[1]);      // Up (positive Y)

    if (Keyboard::IsKeyPressing(Rain::Key::W))
    {
      transform.Translation += front * velocity;
    }
    else if (Keyboard::IsKeyPressing(Rain::Key::S))
    {
      transform.Translation -= front * velocity;
    }
    else if (Keyboard::IsKeyPressing(Rain::Key::A))
    {
      transform.Translation -= right * velocity;
    }
    else if (Keyboard::IsKeyPressing(Rain::Key::D))
    {
      transform.Translation += right * velocity;
    }
    else if (Keyboard::IsKeyPressing(Rain::Key::Space))
    {
      transform.Translation += up * velocity;
    }
    else if (Keyboard::IsKeyPressing(Rain::Key::LeftShift))
    {
      transform.Translation -= up * velocity;
    }
    else if (Keyboard::IsKeyPressing(Rain::Key::Left))
    {
      transform.Translation.x += 0.5;
    }
    else if (Keyboard::IsKeyPressing(Rain::Key::Right))
    {
      transform.Translation.x += -0.5;
    }
    else if (Keyboard::IsKeyPressing(Rain::Key::Up))
    {
      transform.Translation.z += 0.5;
    }
    else if (Keyboard::IsKeyPressing(Rain::Key::Down))
    {
      transform.Translation.z += -0.5;
    }
    else if (Keyboard::IsKeyPressing(Rain::Key::K))
    {
      transform.Translation.y += 0.5;
    }
    else if (Keyboard::IsKeyPressing(Rain::Key::J))
    {
      transform.Translation.y -= 0.5;
    }
    else
    {
      return;
    }
  }
  void Scene::OnMouseMove(double xPos, double yPos)
  {
    static glm::vec2 prevCursorPos = Cursor::GetCursorPosition();
    static bool firstMouse = true;
    static float yaw = -90.0f;  // Start facing forward
    static float pitch = 0.0f;

    if (!Cursor::IsMouseCaptured())
    {
      firstMouse = true;
      return;
    }

    glm::vec2 cursorPos = glm::vec2(xPos, yPos);

    if (firstMouse)
    {
      prevCursorPos = cursorPos;
      firstMouse = false;
      return;
    }

    glm::vec2 delta = cursorPos - prevCursorPos;
    prevCursorPos = cursorPos;

    float sensitivity = 0.1f;
    delta *= sensitivity;

    yaw -= delta.x;
    pitch += delta.y;

    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    Entity cam = GetMainCameraEntity();
    auto& transform = cam.Transform();

    glm::quat yawQuat = glm::angleAxis(glm::radians(yaw), glm::vec3(0, 1, 0));
    glm::quat pitchQuat = glm::angleAxis(glm::radians(pitch), glm::vec3(1, 0, 0));

    transform.Rotation = yawQuat * pitchQuat;
  }
}  // namespace Rain
