#include "Scene.h"
#include "Application.h"
#include "Components.h"
#include "Entity.h"
#include "SceneRenderer.h"
#include "debug/Profiler.h"
#include "glm/gtc/type_ptr.hpp"
#include "imgui.h"
#include "io/cursor.h"
#include "io/keyboard.h"
#include "physics/Physics.h"
#include "render/ResourceManager.h"

#include "ImGuizmo.h"

// Jolt includes

namespace Rain {
  Scene* Scene::Instance = nullptr;

  Scene::Scene(std::string sceneName)
      : m_Name(sceneName) {}

  Entity Scene::CreateEntity(std::string name) {
    return CreateChildEntity({}, name);
  }

  float rotX = 140.0;
  float rotY = 0.0;
  float rotZ = 25.0;

  UUID entityIdDir = 0.0;
  UUID Select = 0;
  Entity entityCamera;
  Entity helment;

  void Scene::Init() {
    Instance = this;
    m_SceneCamera = std::make_unique<PlayerCamera>();

    auto boxModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/box.gltf");
    auto bochii = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/bochii/bochii.gltf");

    helment = CreateEntity("Box");
    Entity floorEntity = CreateEntity("Box2");
    Entity bump = CreateEntity("bump");
    Entity tankEntity = CreateEntity("Tank");

    floorEntity.Transform().Translation = glm::vec3(0, -10, 0);
    floorEntity.Transform().Scale = glm::vec3(100, 1.0f, 100);

    floorEntity.AddComponent<BoxColliderComponent>(glm::vec3(0), glm::vec3(100, 1, 100));
    floorEntity.AddComponent<RigidBodyComponent>();

    BuildMeshEntityHierarchy(floorEntity, boxModel);
    BuildMeshEntityHierarchy(bump, bochii);

    Entity light = CreateEntity("DirectionalLight");
    light.GetComponent<TransformComponent>().SetRotationEuler(glm::vec3(glm::radians(rotX), glm::radians(rotY), glm::radians(rotZ)));
    light.AddComponent<DirectionalLightComponent>();
    entityIdDir = light.GetUUID();

    Physics::Instance = new Physics();
    m_PhysicsScene = Physics::Instance->CreateScene(glm::vec3(0.0, -9.8f, 0.0));
    m_PhysicsScene->CreateBody(floorEntity);
  }

  Entity Scene::CreateChildEntity(Entity parent, std::string name) {
    Entity entity = Entity(m_World.entity(), this);
    uint32_t entityHandle = entity;

    entity.AddComponent<TransformComponent>();
    if (!name.empty()) {
      TagComponent& tagComponent = entity.AddComponent<TagComponent>();
      tagComponent.Tag = name;
    }
    entity.AddComponent<RelationshipComponent>();
    IDComponent& idComponent = entity.AddComponent<IDComponent>();
    idComponent.ID = {};

    if (parent) {
      entity.SetParent(parent);
    }

    SceneLightInfo.LightPos = glm::vec3(0.0f);
    m_EntityMap[idComponent.ID] = entity;

    return entity;
  }

  Entity Scene::TryGetEntityWithUUID(UUID id) const {
    if (const auto iter = m_EntityMap.find(id); iter != m_EntityMap.end()) {
      return iter->second;
    }
    return {};
  }

  std::pair<glm::vec3, glm::vec3> Scene::CastRay(const PlayerCamera& camera, float mx, float my) {
    // Convert mouse coordinates to normalized device coordinates (NDC)
    float w = Application::Get()->GetWindowSize().x;
    float h = Application::Get()->GetWindowSize().y;

    // Convert to clip space coordinates (-1 to 1 range)
    float x = (2.0f * mx / w) - 1.0f;
    float y = 1.0f - (2.0f * my / h);  // Invert Y for WebGPU/OpenGL

    glm::vec4 clipPos = glm::vec4(x, y, -1.0f, 1.0f);

    // Get current projection and view matrices (don't use static)
    glm::mat4 projMatrix = glm::perspectiveFov(glm::radians(55.0f), w, h, 0.10f, 1400.0f);
    glm::mat4 viewMatrix = camera.GetViewMatrix();

    // Calculate inverse matrices
    glm::mat4 invProj = glm::inverse(projMatrix);
    glm::mat4 invView = glm::inverse(viewMatrix);

    // Unproject the clip space position to view space
    glm::vec4 rayEye = invProj * clipPos;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);  // Reset z and set w=0 for direction

    // Transform to world space
    glm::vec4 rayWorld = invView * rayEye;
    glm::vec3 rayDir = glm::normalize(glm::vec3(rayWorld));

    // Ray origin is the camera position
    glm::vec3 rayPos = camera.Position;

    return {rayPos, rayDir};
  }

  void Scene::OnUpdate() {
    ScanKeyPress();

    if (Cursor::WasLeftCursorPressed()) {
      SceneQueryHit hit;
      auto [origin, direction] = CastRay(*m_SceneCamera, Cursor::GetCursorPosition().x, Cursor::GetCursorPosition().y);
      RayCastInfo info;
      info.Origin = origin;
      info.Direction = direction;
      info.MaxDistance = 1000;

      if (m_PhysicsScene->CastRay(&info, hit)) {
        RN_LOG("Success Entity {}", hit.HitEntity);
        Select = hit.HitEntity;
      }
    }

    m_PhysicsScene->Update(1.0f / 60.0);
    Cursor::Update();
  }

  glm::mat4 Scene::EditTransform(glm::mat4& matrix) {
    static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::ROTATE);
    static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);

    // Handle keyboard shortcuts
    if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
      mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E)) {
      mCurrentGizmoOperation = ImGuizmo::ROTATE;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R)) {
      mCurrentGizmoOperation = ImGuizmo::SCALE;
    }

    // Prepare matrices for manipulation
    float modelMatrix[16];
    std::memcpy(modelMatrix, glm::value_ptr(matrix), sizeof(float) * 16);

    // Snapping controls
    static bool useSnap(false);
    if (ImGui::IsKeyPressed(ImGuiKey_S)) {
      useSnap = !useSnap;
    }
    glm::vec3 snap(0.0f);

    // Setup manipulation environment
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    // Get view and projection matrices
    const glm::mat4& viewMat = m_SceneCamera->GetViewMatrix();
    static float w = Application::Get()->GetWindowSize().x;
    static float h = Application::Get()->GetWindowSize().y;
    static const glm::mat4 projMat = glm::perspectiveFov(glm::radians(55.0f), w, h, 0.10f, 1400.0f);

    float viewMatrix[16], projectionMatrix[16];
    std::memcpy(viewMatrix, glm::value_ptr(viewMat), sizeof(float) * 16);
    std::memcpy(projectionMatrix, glm::value_ptr(projMat), sizeof(float) * 16);

    // Perform manipulation
    ImGuizmo::Manipulate(
        viewMatrix,
        projectionMatrix,
        mCurrentGizmoOperation,
        mCurrentGizmoMode,
        modelMatrix,
        nullptr,
        useSnap ? &snap.x : nullptr);

    // Update the matrix with manipulated result
    matrix = glm::make_mat4(modelMatrix);

    return matrix;
  }

  UUID RenderNode(Entity e, Scene* scene) {
    TagComponent tag = e.GetComponent<TagComponent>();
    static UUID selectedNode = -1;
    UUID currentNode = e.GetComponent<IDComponent>().ID;
    int childCount = 0;
    RelationshipComponent r = {};
    bool isRempty = true;

    if (e.HasComponent<RelationshipComponent>()) {
      r = e.GetComponent<RelationshipComponent>();
      isRempty = true;
      childCount = r.Children.size();
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (childCount == 0) {
      flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (selectedNode == currentNode) {
      flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (ImGui::TreeNodeEx(tag.Tag.c_str(), flags)) {
      if (ImGui::IsItemClicked()) {
        RN_LOG("Name: {} ID: {} C: {}", tag.Tag, (uint64_t)currentNode, (uint64_t)selectedNode);
        selectedNode = currentNode;
      }

      // if (r != {}) {
      if (!isRempty) {
        if (r.Children.size() > 0) {
          ImGui::Indent(0.5f);
        }

        for (auto child : r.Children) {
          Entity childEntity = scene->TryGetEntityWithUUID(child);
          RenderNode(childEntity, scene);
        }
      }

      ImGui::TreePop();
    }

    return selectedNode;
  }

  void Scene::OnRender(Ref<SceneRenderer> renderer) {
    RN_PROFILE_FUNC;
    renderer->SetScene(this);

    static float w = Application::Get()->GetWindowSize().x;
    static float h = Application::Get()->GetWindowSize().y;

    static auto projectionMatrix = glm::perspectiveFov(glm::radians(55.0f), w, h, 0.10f, 1400.0f);

    renderer->BeginScene({m_SceneCamera->GetViewMatrix(), projectionMatrix, 0.10, 1400.0f});

    static flecs::query<TransformComponent, MeshComponent> drawNodeQuery = m_World.query<TransformComponent, MeshComponent>();
    drawNodeQuery.each([&](flecs::entity entity, TransformComponent& transform, MeshComponent& meshComponent) {
      Entity e = Entity(entity, this);
      Ref<MeshSource> meshSource = Rain::ResourceManager::GetMeshSource(meshComponent.MeshSourceId);
      glm::mat4 entityTransform = GetWorldSpaceTransformMatrix(e);

      renderer->SubmitMesh(meshSource, meshComponent.SubMeshId, meshComponent.Materials, entityTransform);
    });

    static flecs::query<TransformComponent, DirectionalLightComponent> lightsQuery = m_World.query<TransformComponent, DirectionalLightComponent>();
    lightsQuery.each([&](flecs::entity entity, TransformComponent& transform, DirectionalLightComponent& directionalLight) {
      // SceneLightInfo.LightDirection = glm::normalize(glm::mat3(transform.GetTransform()) * glm::vec3(1.0f));
      SceneLightInfo.LightDirection = glm::normalize(glm::mat3(transform.GetTransform()) * glm::vec3(0.0f, 0.0f, -1.0f));
      SceneLightInfo.LightPos = glm::vec3(0.0f);
    });

    ImGuizmo::BeginFrame();
    ImGui::Begin("Scene Settings");

    glm::vec3 rotation = glm::vec3(rotX, rotY, rotZ);
    if (ImGui::InputFloat3("Light Direction", glm::value_ptr(rotation), "%.3f")) {
      rotX = rotation.x;
      rotY = rotation.y;
      rotZ = rotation.z;

      auto ent2 = TryGetEntityWithUUID(entityIdDir);
      auto transform2 = ent2.GetComponent<TransformComponent>();
      transform2.SetRotationEuler(glm::radians(rotation));
    }

    static float metallic = 0.0, roughness = 0.0, ao = 0.0;

    // if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) {
    //   auto mat = cityModel->Materials->GetMaterial(0);
    //   mat->Set("Metallic", metallic);
    // }

    // if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) {
    //   auto mat = cityModel->Materials->GetMaterial(0);
    //   mat->Set("Roughness", roughness);
    // }

    // if (ImGui::SliderFloat("Ao", &ao, 0.0f, 1.0f)) {
    //   auto mat = cityModel->Materials->GetMaterial(0);
    //   mat->Set("Ao", ao);
    // }

    ImGui::End();

    auto pt = GetWorldSpaceTransformMatrix(helment);

    auto v = m_SceneCamera->GetViewMatrix();

    static auto projectionMatrix1 = glm::perspectiveFov(glm::radians(55.0f), w, h, 0.10f, 1400.0f);

    // ImGui::Begin("Hierarchy");

    // m_World.each([this](flecs::entity e, IDComponent& id, RelationshipComponent& r, TagComponent& t) {
    //   Entity entity = TryGetEntityWithUUID(id.ID);
    //   if (r.ParentHandle == 0) {
    //     auto uuid = RenderNode(entity, this);

    //    if (Select != 0 && Select == id.ID) {
    //      auto pt = entity.GetComponent<TransformComponent>()->GetTransform();
    //      auto tm = EditTransform(pt);

    //      auto tc = entity.GetComponent<TransformComponent>();
    //      tc->SetTransform(tm);
    //      auto body = m_PhysicsScene->GetEntityBodyByID(Select);
    //      if (body != 0) {
    //        body->SetTranslation(tc->Translation);
    //      }
    //    }
    //  }
    //});

    // ImGui::End();

    renderer->EndScene();
  }

  void Scene::BuildMeshEntityHierarchy(Entity parent, Ref<MeshSource> mesh) {
    const std::vector<Ref<MeshNode>> nodes = mesh->GetNodes();

    for (const Ref<MeshNode> node : mesh->GetNodes()) {
      if (node->IsRoot() && mesh->m_SubMeshes[node->SubMeshId].VertexCount == 0) {
        continue;
      }
      Entity nodeEntity = CreateChildEntity(parent, node->Name);
      uint32_t materialId = mesh->m_SubMeshes[node->SubMeshId].MaterialIndex;

      nodeEntity.AddComponent<MeshComponent>(mesh->Id, (uint32_t)node->SubMeshId, materialId);
      nodeEntity.AddComponent<TransformComponent>();

      nodeEntity.Transform().SetTransform(node->LocalTransform);
    }
  }

  glm::mat4 Scene::GetWorldSpaceTransformMatrix(Entity entity) {
    glm::mat4 transform = glm::identity<glm::mat4>();

    Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());
    if (parent) {
      transform = GetWorldSpaceTransformMatrix(parent);
    }

    return transform * entity.GetComponent<TransformComponent>().GetTransform();
  }

  void Scene::ConvertToLocalSpace(Entity entity) {
    Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());

    if (!parent) {
      return;
    }

    TransformComponent& transform = entity.Transform();
    glm::mat4 parentTransform = GetWorldSpaceTransformMatrix(parent);
    glm::mat4 localTransform = glm::inverse(parentTransform) * transform.GetTransform();
    transform.SetTransform(localTransform);
  }

  TransformComponent Scene::GetWorldSpaceTransform(Entity entity) {
    glm::mat4 transform = GetWorldSpaceTransformMatrix(entity);
    TransformComponent transformComponent;
    transformComponent.SetTransform(transform);
    return transformComponent;
  }

  void Scene::ScanKeyPress() {
    float speed = 0.55f;

    if (Keyboard::IsKeyPressing(Rain::Key::W)) {
      m_SceneCamera->ProcessKeyboard(FORWARD, speed);
    } else if (Keyboard::IsKeyPressing(Rain::Key::S)) {
      m_SceneCamera->ProcessKeyboard(BACKWARD, speed);
    } else if (Keyboard::IsKeyPressing(Rain::Key::A)) {
      m_SceneCamera->ProcessKeyboard(LEFT, speed);
    } else if (Keyboard::IsKeyPressing(Rain::Key::D)) {
      m_SceneCamera->ProcessKeyboard(RIGHT, speed);
    } else if (Keyboard::IsKeyPressing(Rain::Key::Space)) {
      m_SceneCamera->ProcessKeyboard(UP, speed);
    } else if (Keyboard::IsKeyPressing(Rain::Key::LeftShift)) {
      m_SceneCamera->ProcessKeyboard(DOWN, speed);
    } else {
      return;
    }

    auto& pos = m_SceneCamera->Position;
  }

  void Scene::OnMouseMove(double xPos, double yPos) {
    static glm::vec2 prevCursorPos = Cursor::GetCursorPosition();
    static bool a = false;

    if (Keyboard::IsKeyPressed(Key::F)) {
      a = !a;
      if (a) {
        Cursor::CaptureMouse(false);
      }
    }

    if (a) {
      return;
    }
    if (!Cursor::IsMouseCaptured()) {
      return;
    }
    glm::vec2 cursorPos = Cursor::GetCursorPosition();
    m_SceneCamera->ProcessMouseMovement(cursorPos.x - prevCursorPos.x, cursorPos.y - prevCursorPos.y);
    prevCursorPos = cursorPos;
  }
}  // namespace Rain
