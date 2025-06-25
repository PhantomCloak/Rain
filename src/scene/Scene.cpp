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
    m_SceneCamera->Position = glm::vec3(0, 10, 0);
    m_SceneCamera->Pitch = -89;

    auto boxModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/box.gltf");
    auto bochii = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/galata/galata.gltf");

    helment = CreateEntity("Box");
    Entity floorEntity = CreateEntity("Box2");
    Entity bump = CreateEntity("bump");
    Entity tankEntity = CreateEntity("Tank");

    bump.Transform().Translation = glm::vec3(0, 0, 0);
    bump.Transform().Scale = glm::vec3(0.05f);

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

  // void Scene::OnRender(Ref<SceneRenderer> renderer) {
  //   RN_PROFILE_FUNC;
  //   renderer->SetScene(this);

  //  static float w = Application::Get()->GetWindowSize().x;
  //  static float h = Application::Get()->GetWindowSize().y;

  //  static auto projectionMatrix = glm::perspectiveFov(glm::radians(55.0f), w, h, 0.10f, 1400.0f);

  //  renderer->BeginScene({m_SceneCamera->GetViewMatrix(), projectionMatrix, 0.10, 1400.0f});

  //  static flecs::query<TransformComponent, MeshComponent> drawNodeQuery = m_World.query<TransformComponent, MeshComponent>();
  //  drawNodeQuery.each([&](flecs::entity entity, TransformComponent& transform, MeshComponent& meshComponent) {
  //    Entity e = Entity(entity, this);
  //    Ref<MeshSource> meshSource = Rain::ResourceManager::GetMeshSource(meshComponent.MeshSourceId);
  //    glm::mat4 entityTransform = GetWorldSpaceTransformMatrix(e);

  //    renderer->SubmitMesh(meshSource, meshComponent.SubMeshId, meshComponent.Materials, entityTransform);
  //  });

  //  static flecs::query<TransformComponent, DirectionalLightComponent> lightsQuery = m_World.query<TransformComponent, DirectionalLightComponent>();
  //  lightsQuery.each([&](flecs::entity entity, TransformComponent& transform, DirectionalLightComponent& directionalLight) {
  //    // SceneLightInfo.LightDirection = glm::normalize(glm::mat3(transform.GetTransform()) * glm::vec3(1.0f));
  //    SceneLightInfo.LightDirection = glm::normalize(glm::mat3(transform.GetTransform()) * glm::vec3(0.0f, 0.0f, -1.0f));
  //    SceneLightInfo.LightPos = glm::vec3(0.0f);
  //  });

  //  renderer->EndScene();
  //}

  void Scene::OnRender(Ref<SceneRenderer> renderer) {
    RN_PROFILE_FUNC;
    renderer->SetScene(this);
    static float w = Application::Get()->GetWindowSize().x;
    static float h = Application::Get()->GetWindowSize().y;

    // static float parallaxOffsetX = 0.0f;
    // static float parallaxOffsetY = 0.0f;
    // static float screenDistance = 10.0f;
    // static bool useCameraOrbit = true;
    // static bool useAsymmetricProjection = true;

    // static float maxOrbitAngle = 45.0f;

    // auto createParallaxProjection = [&](float offsetX, float offsetY) -> glm::mat4 {
    //   float fov = glm::radians(55.0f);
    //   float aspect = w / h;
    //   float near = 0.10f;
    //   float far = 1400.0f;

    //  float top = near * tan(fov * 0.5f);
    //  float bottom = -top;
    //  float right = top * aspect;
    //  float left = -right;

    //  float parallaxX = offsetX * near / screenDistance;
    //  float parallaxY = offsetY * near / screenDistance;

    //  left += parallaxX;
    //  right += parallaxX;
    //  bottom += parallaxY;
    //  top += parallaxY;

    //  return glm::frustum(left, right, bottom, top, near, far);
    //};

    // glm::mat4 projectionMatrix;
    // if (useAsymmetricProjection) {
    //   projectionMatrix = createParallaxProjection(parallaxOffsetX, parallaxOffsetY);
    // } else {
    //   projectionMatrix = glm::perspectiveFov(glm::radians(55.0f), w, h, 0.10f, 500.0f);
    // }

    // glm::mat4 viewMatrix;
    // if (useCameraOrbit) {
    //   glm::mat4 originalView = m_SceneCamera->GetViewMatrix();
    //   glm::mat4 invView = glm::inverse(originalView);
    //   glm::vec3 originalCameraPos = glm::vec3(invView[3]);
    //   glm::vec3 originalForward = -glm::vec3(invView[2]);
    //   glm::vec3 originalUp = glm::vec3(invView[1]);
    //   glm::vec3 originalRight = glm::vec3(invView[0]);

    //  float yawAngle = (parallaxOffsetX / 50.0f) * glm::radians(maxOrbitAngle);

    //  // Apply yaw rotation (around up vector) to forward direction
    //  glm::mat4 yawRotation = glm::rotate(glm::mat4(1.0f), yawAngle, originalUp);
    //  glm::vec3 rotatedForward = glm::vec3(yawRotation * glm::vec4(originalForward, 0.0f));
    //  glm::vec3 rotatedRight = glm::vec3(yawRotation * glm::vec4(originalRight, 0.0f));

    //  // === Y LOGIC - SIMPLIFIED AND FIXED ===
    //  glm::vec3 finalCameraPos = originalCameraPos;

    //  // Always start with X logic for target (preserves working X behavior)
    //  float lookDistance = 100.0f;
    //  glm::vec3 finalTarget = originalCameraPos + rotatedForward * lookDistance;

    //  // Only apply Y orbit if Y offset is active
    //  // if (abs(parallaxOffsetY) > 0.1f) {
    //  //  // Calculate vertical orbit angle
    //  //  float verticalAngle = (parallaxOffsetY / 50.0f) * glm::radians(maxOrbitAngle * 0.2f);  // Even smaller range
    //  //  verticalAngle = glm::clamp(verticalAngle, glm::radians(-15.0f), glm::radians(15.0f));  // Strict clamp

    //  //  // Simple vertical offset: just move camera up/down slightly
    //  //  finalCameraPos.y = originalCameraPos.y + (parallaxOffsetY * 0.1f);

    //  //  // Calculate orbit center based on current forward direction
    //  //  glm::vec3 orbitCenter = originalCameraPos + rotatedForward * lookDistance;

    //  //  // For Y movement, always look at the orbit center to keep it in view
    //  //  finalTarget = orbitCenter;
    //  //}

    //  // Original camera position offset for enhanced parallax (X logic)
    //  float cameraOffsetStrength = 2.0f;
    //  glm::vec3 cameraOffset =
    //      rotatedRight * (parallaxOffsetX * cameraOffsetStrength * 0.1f) +
    //      originalUp * (parallaxOffsetY * cameraOffsetStrength * 0.1f);

    //  finalCameraPos += cameraOffset;

    //  // Create view matrix
    //  viewMatrix = glm::lookAt(finalCameraPos, finalTarget, originalUp);
    //} else {
    //  // Use original view matrix
    //  viewMatrix = m_SceneCamera->GetViewMatrix();
    //}

    auto far = 400.0f;
    auto viewMatrix = m_SceneCamera->GetViewMatrix();
    auto projectionMatrix = glm::perspectiveFov(glm::radians(55.0f), w, h, 0.10f, far);
    renderer->BeginScene({viewMatrix, projectionMatrix, 0.10, far});

    static flecs::query<TransformComponent, MeshComponent> drawNodeQuery = m_World.query<TransformComponent, MeshComponent>();
    drawNodeQuery.each([&](flecs::entity entity, TransformComponent& transform, MeshComponent& meshComponent) {
      Entity e = Entity(entity, this);
      Ref<MeshSource> meshSource = Rain::ResourceManager::GetMeshSource(meshComponent.MeshSourceId);
      glm::mat4 entityTransform = GetWorldSpaceTransformMatrix(e);
      renderer->SubmitMesh(meshSource, meshComponent.SubMeshId, meshComponent.Materials, entityTransform);
    });

    static flecs::query<TransformComponent, DirectionalLightComponent> lightsQuery = m_World.query<TransformComponent, DirectionalLightComponent>();
    lightsQuery.each([&](flecs::entity entity, TransformComponent& transform, DirectionalLightComponent& directionalLight) {
      SceneLightInfo.LightDirection = glm::normalize(glm::mat3(transform.GetTransform()) * glm::vec3(0.0f, 0.0f, -1.0f));
      SceneLightInfo.LightPos = glm::vec3(0.0f);
    });

    // ImGui::Begin("Scene Settings");
    // ImGui::Separator();
    // ImGui::Text("3D Parallax Effect");

    // ImGui::Checkbox("Use Camera Orbiting", &useCameraOrbit);
    // ImGui::Checkbox("Use Asymmetric Projection", &useAsymmetricProjection);

    // if (!useCameraOrbit && !useAsymmetricProjection) {
    //   ImGui::TextColored(ImVec4(1, 1, 0, 1), "Warning: Both effects disabled!");
    // }

    // ImGui::Separator();
    // ImGui::Text("Parallax Controls");

    // if (ImGui::SliderFloat("Screen Distance", &screenDistance, 10.0f, 500.0f)) {
    // }

    // if (ImGui::SliderFloat("Max Orbit Angle", &maxOrbitAngle, 10.0f, 90.0f)) {
    // }

    // ImGui::Text("Current Offset: (%.2f, %.2f)", parallaxOffsetX, parallaxOffsetY);

    // if (ImGui::Button("Reset Parallax")) {
    //   parallaxOffsetX = 0.0f;
    //   parallaxOffsetY = 0.0f;
    // }

    // ImGui::SliderFloat("Manual Offset X (Head Turn)", &parallaxOffsetX, -90.0f, 90.0f);
    // ImGui::SliderFloat("Manual Offset Y (Vertical Orbit)", &parallaxOffsetY, -90.0f, 90.0f);

    // glm::mat4 invView = glm::inverse(viewMatrix);
    // glm::vec3 currentCamPos = glm::vec3(invView[3]);
    // ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", currentCamPos.x, currentCamPos.y, currentCamPos.z);
    // ImGui::Text("Camera Yaw: %.2f Pitch: %.2f", m_SceneCamera->Yaw, m_SceneCamera->Pitch);

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

    if (!Cursor::IsMouseCaptured()) {
      return;
    }

    glm::vec2 cursorPos = Cursor::GetCursorPosition();
    m_SceneCamera->ProcessMouseMovement(cursorPos.x - prevCursorPos.x, cursorPos.y - prevCursorPos.y);
    prevCursorPos = cursorPos;
  }
}  // namespace Rain
