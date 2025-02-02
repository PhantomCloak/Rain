#include "Scene.h"
#include "Application.h"
#include "Components.h"
#include "Entity.h"
#include "SceneRenderer.h"
#include "debug/Profiler.h"
#include "glm/gtc/type_ptr.hpp"
//#include "imgui.h"
#include "io/cursor.h"
#include "io/keyboard.h"
#include "render/ResourceManager.h"
// #include <Tracy.hpp>
//#include "ImGuizmo/ImGuizmo.h"

Scene::Scene(std::string sceneName)
    : m_Name(sceneName) {}

Entity Scene::CreateEntity(std::string name) {
  return CreateChildEntity({}, name);
}

float rotX = 90.0;
float rotY = 0.0;
float rotZ = 25.0;

UUID entityIdDir = 0.0;
Entity entityCamera;
Ref<MeshSource> cityModel;
Entity map;

void Scene::Init() {
  m_SceneCamera = std::make_unique<PlayerCamera>();
  //m_SceneCamera->Position.x = -105.414;
  //m_SceneCamera->Position.y = 72.52;
  //m_SceneCamera->Position.z = -139.405;
  //m_SceneCamera->Yaw = 128.699;
  //m_SceneCamera->Pitch = -24.00;

  //cityModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/models/cityz.gltf");
  cityModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/Helment/untitled.gltf");
	auto cityModel2 = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/box.gltf");
  //  cityModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/glTF/DamagedHelmet.gltf");
  //  cityModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/weapon.gltf");
  //  cityModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/sponza/SponzaExp5.gltf");
  //  cityModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/test.gltf");
  //  cityModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/assault_rifle_pbr/scene.gltf");
  //  cityModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/pbr_spheres/scene.gltf");
  map = CreateEntity("Box");
  auto map2 = CreateEntity("Box2");
  // map.GetComponent<TransformComponent>()->Translation = glm::vec3(0.0, 0.0, 0);
  // map.GetComponent<TransformComponent>()->Scale = glm::vec3(300.0f);
  // map.GetComponent<TransformComponent>()->Scale = glm::vec3(0.10f);
  // map.GetComponent<TransformComponent>()->Scale = glm::vec3(25.0f);
  // map.GetComponent<TransformComponent>()->Scale = glm::vec3(350.0f);
   map.GetComponent<TransformComponent>()->Scale = glm::vec3(1.0f);
   map2.GetComponent<TransformComponent>()->Scale = glm::vec3(1.0f);
  // map.GetComponent<TransformComponent>()->SetRotationEuler(glm::vec3(glm::radians(90.0), glm::radians(0.0), glm::radians(180.0)));
  //map.GetComponent<TransformComponent>()->Scale = glm::vec3(0.10f);
  BuildMeshEntityHierarchy(map, cityModel);
  BuildMeshEntityHierarchy(map2, cityModel2);

  // auto mat = cityModel->Materials->GetMaterial(0);
  // mat->Set("Metallic", 0.1);
  // mat->Set("Roughness", 1.0);

  Entity light = CreateEntity("DirectionalLight");
  light.GetComponent<TransformComponent>()->SetRotationEuler(glm::vec3(glm::radians(rotX), glm::radians(rotY), glm::radians(rotZ)));
  light.AddComponent<DirectionalLightComponent>();
  entityIdDir = light.GetUUID();
}

Entity Scene::CreateChildEntity(Entity parent, std::string name) {
  auto entity = Entity(m_World.entity(), this);
  uint32_t entityHandle = entity;

  entity.AddComponent<TransformComponent>();
  if (!name.empty()) {
    TagComponent* tagComponent = entity.AddComponent<TagComponent>();
    tagComponent->Tag = name;
  }
  entity.AddComponent<RelationshipComponent>();
  IDComponent* idComponent = entity.AddComponent<IDComponent>();
  idComponent->ID = {};

  if (parent) {
    entity.SetParent(parent);
  }

  SceneLightInfo.LightPos = glm::vec3(0.0f);
  m_EntityMap[idComponent->ID] = entity;

  return entity;
}

Entity Scene::TryGetEntityWithUUID(UUID id) const {
  if (const auto iter = m_EntityMap.find(id); iter != m_EntityMap.end()) {
    return iter->second;
  }
  return {};
}

void Scene::OnUpdate() {
  ScanKeyPress();
}

//glm::mat4 Scene::EditTransform(glm::mat4& matrix) {
//  static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::ROTATE);
//  static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
//
//  // Handle keyboard shortcuts
//  if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
//    mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
//  }
//  if (ImGui::IsKeyPressed(ImGuiKey_E)) {
//    mCurrentGizmoOperation = ImGuizmo::ROTATE;
//  }
//  if (ImGui::IsKeyPressed(ImGuiKey_R)) {
//    mCurrentGizmoOperation = ImGuizmo::SCALE;
//  }
//
//  // Prepare matrices for manipulation
//  float modelMatrix[16];
//  std::memcpy(modelMatrix, glm::value_ptr(matrix), sizeof(float) * 16);
//
//  // Snapping controls
//  static bool useSnap(false);
//  if (ImGui::IsKeyPressed(ImGuiKey_S)) {
//    useSnap = !useSnap;
//  }
//  glm::vec3 snap(0.0f);
//
//  // Setup manipulation environment
//  ImGuiIO& io = ImGui::GetIO();
//  ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
//
//  // Get view and projection matrices
//  const glm::mat4& viewMat = m_SceneCamera->GetViewMatrix();
//  static float w = Application::Get()->GetWindowSize().x;
//  static float h = Application::Get()->GetWindowSize().y;
//  static const glm::mat4 projMat = glm::perspectiveFov(glm::radians(55.0f), w, h, 0.10f, 1400.0f);
//
//  float viewMatrix[16], projectionMatrix[16];
//  std::memcpy(viewMatrix, glm::value_ptr(viewMat), sizeof(float) * 16);
//  std::memcpy(projectionMatrix, glm::value_ptr(projMat), sizeof(float) * 16);
//
//  // Perform manipulation
//  ImGuizmo::Manipulate(
//      viewMatrix,
//      projectionMatrix,
//      mCurrentGizmoOperation,
//      mCurrentGizmoMode,
//      modelMatrix,
//      nullptr,
//      useSnap ? &snap.x : nullptr);
//
//  // Update the matrix with manipulated result
//  matrix = glm::make_mat4(modelMatrix);
//
//	return matrix;
//}
//UUID RenderNode(Entity e, Scene* scene) {
//  auto tag = e.GetComponent<TagComponent>();
//  static UUID selectedNode = -1;
//  UUID currentNode = e.GetComponent<IDComponent>()->ID;
//  int childCount = 0;
//  RelationshipComponent* r = nullptr;
//
//  if (e.HasComponent<RelationshipComponent>()) {
//    r = e.GetComponent<RelationshipComponent>();
//    childCount = r->Children.size();
//  }
//
//  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
//  if (childCount == 0) {
//    flags |= ImGuiTreeNodeFlags_Leaf;
//  }
//  if (selectedNode == currentNode) {
//    flags |= ImGuiTreeNodeFlags_Selected;
//  }
//
//  if (ImGui::TreeNodeEx(tag->Tag.c_str(), flags)) {
//    if (ImGui::IsItemClicked()) {
//      RN_LOG("Name: {} ID: {} C: {}", tag->Tag, (uint64_t)currentNode, (uint64_t)selectedNode);
//      selectedNode = currentNode;
//    }
//
//    if (r != nullptr) {
//      if (r->Children.size() > 0) {
//        ImGui::Indent(0.5f);
//      }
//
//      for (auto child : r->Children) {
//        Entity childEntity = scene->TryGetEntityWithUUID(child);
//        RenderNode(childEntity, scene);
//      }
//    }
//
//    ImGui::TreePop();
//  }
//
//  return selectedNode;
//}

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

  //ImGuizmo::BeginFrame();
  //ImGui::Begin("Scene Settings");

  //glm::vec3 rotation = glm::vec3(rotX, rotY, rotZ);
  //if (ImGui::InputFloat3("Light Direction", glm::value_ptr(rotation), "%.3f")) {
  //  rotX = rotation.x;
  //  rotY = rotation.y;
  //  rotZ = rotation.z;

  //  auto ent2 = TryGetEntityWithUUID(entityIdDir);
  //  auto transform2 = ent2.GetComponent<TransformComponent>();
  //  transform2->SetRotationEuler(glm::radians(rotation));
  //}

  //static float metallic = 0.0, roughness = 0.0, ao = 0.0;

  //if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) {
  //  auto mat = cityModel->Materials->GetMaterial(0);
  //  mat->Set("Metallic", metallic);
  //}

  //if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) {
  //  auto mat = cityModel->Materials->GetMaterial(0);
  //  mat->Set("Roughness", roughness);
  //}

  //if (ImGui::SliderFloat("Ao", &ao, 0.0f, 1.0f)) {
  //  auto mat = cityModel->Materials->GetMaterial(0);
  //  mat->Set("Ao", ao);
  //}

  //ImGui::End();

  //auto pt = GetWorldSpaceTransformMatrix(map);

  //auto v = m_SceneCamera->GetViewMatrix();

  //static auto projectionMatrix1 = glm::perspectiveFov(glm::radians(55.0f), w, h, 0.10f, 1400.0f);

  //ImGui::Begin("Hierarchy");

  //m_World.each([this](flecs::entity e, IDComponent& id, RelationshipComponent& r, TagComponent& t) {
  //  Entity entity = TryGetEntityWithUUID(id.ID);
  //  if (r.ParentHandle == 0) {
  //    auto uuid = RenderNode(entity, this);

  //    if (uuid == id.ID) {
  //      auto pt = entity.GetComponent<TransformComponent>()->GetTransform();
  //      auto tm = EditTransform(pt);
	//			entity.GetComponent<TransformComponent>()->SetTransform(tm);
  //    }
  //  }
  //});

  //ImGui::End();

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

    nodeEntity.Transform()->SetTransform(node->LocalTransform);
  }
}

glm::mat4 Scene::GetWorldSpaceTransformMatrix(Entity entity) {
  glm::mat4 transform = glm::identity<glm::mat4>();

  Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());
  if (parent) {
    transform = GetWorldSpaceTransformMatrix(parent);
  }

  return transform * entity.GetComponent<TransformComponent>()->GetTransform();
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
