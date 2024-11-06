#include "Scene.h"
#include "Application.h"
#include "Components.h"
#include "Entity.h"
#include "SceneRenderer.h"
#include "debug/Profiler.h"
#include "imgui.h"
#include "io/cursor.h"
#include "io/keyboard.h"
#include "render/ResourceManager.h"
// #include <Tracy.hpp>

Scene::Scene(std::string sceneName)
    : m_Name(sceneName) {}

Entity Scene::CreateEntity(std::string name) {
  return CreateChildEntity({}, name);
}

float rotX = 85.0;
float rotY = 0.0;
float rotZ = -15.0;

UUID entityIdDir = 0.0;
Entity entityCamera;
void Scene::Init() {
  m_SceneCamera = std::make_unique<PlayerCamera>();
	m_SceneCamera->Position.x = 198.84;
  m_SceneCamera->Position.y = 90.84;
  m_SceneCamera->Position.z = 155;

  Ref<MeshSource> cityModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/models/city-test4.gltf");
  Entity map = CreateEntity("Box");
  map.GetComponent<TransformComponent>()->Translation = glm::vec3(0.0, 0.0, 0);
  map.GetComponent<TransformComponent>()->Scale = glm::vec3(0.10);
  BuildMeshEntityHierarchy(map, cityModel);

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

void Scene::OnRender(Ref<SceneRenderer> renderer) {
  RN_PROFILE_FUNC;
  renderer->SetScene(this);

  static float w = Application::Get()->GetWindowSize().x;
  static float h = Application::Get()->GetWindowSize().y;

  static auto projectionMatrix = glm::perspectiveFov(glm::radians(55.0f), w, h, 0.10f, 1500.0f);

  renderer->BeginScene({m_SceneCamera->GetViewMatrix(), projectionMatrix, 1.10, 1500.0f});

  static flecs::query<TransformComponent, MeshComponent> drawNodeQuery = m_World.query<TransformComponent, MeshComponent>();
  drawNodeQuery.each([&](flecs::entity entity, TransformComponent& transform, MeshComponent& meshComponent) {
    Entity e = Entity(entity, this);
    Ref<MeshSource> meshSource = Rain::ResourceManager::GetMeshSource(meshComponent.MeshSourceId);
    glm::mat4 entityTransform = GetWorldSpaceTransformMatrix(e);

    renderer->SubmitMesh(meshSource, meshComponent.SubMeshId, meshComponent.Materials, entityTransform);
  });

  static flecs::query<TransformComponent, DirectionalLightComponent> lightsQuery = m_World.query<TransformComponent, DirectionalLightComponent>();
  lightsQuery.each([&](flecs::entity entity, TransformComponent& transform, DirectionalLightComponent& directionalLight) {
    //SceneLightInfo.LightDirection = glm::normalize(glm::mat3(transform.GetTransform()) * glm::vec3(1.0f));
		SceneLightInfo.LightDirection = glm::normalize(glm::mat3(transform.GetTransform()) * glm::vec3(0.0f, 0.0f, -1.0f));
		SceneLightInfo.LightPos = glm::vec3(0.0f);
  });

  ImGui::Begin("Scene Settings");

  if (ImGui::InputFloat("Rot X", &rotX, 0, 1)) {
    auto ent2 = TryGetEntityWithUUID(entityIdDir);

    auto transform2 = ent2.GetComponent<TransformComponent>();

    transform2->SetRotationEuler(glm::vec3(glm::radians(rotX), glm::radians(rotY), glm::radians(rotZ)));
  }
  if (ImGui::InputFloat("Rot Y", &rotY, 0, 1)) {
    auto ent2 = TryGetEntityWithUUID(entityIdDir);

    auto transform2 = ent2.GetComponent<TransformComponent>();
    transform2->SetRotationEuler(glm::vec3(glm::radians(rotX), glm::radians(rotY), glm::radians(rotZ)));
  }
  if (ImGui::InputFloat("Rot Z", &rotZ, 0, 1)) {
    auto ent2 = TryGetEntityWithUUID(entityIdDir);

    auto transform2 = ent2.GetComponent<TransformComponent>();
    transform2->SetRotationEuler(glm::vec3(glm::radians(rotX), glm::radians(rotY), glm::radians(rotZ)));
  }
  ImGui::End();

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
  float speed = 1.55f;

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
