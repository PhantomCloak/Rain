#include "Scene.h"
#include "Application.h"
#include "Components.h"
#include "Entity.h"
#include "SceneRenderer.h"
#include "io/cursor.h"
#include "io/keyboard.h"
#include "render/ResourceManager.h"

Scene::Scene(std::string sceneName)
    : m_Name(sceneName) {}

Entity Scene::CreateEntity(std::string name) {
  return CreateChildEntity({}, name);
}

void Scene::Init() {
  m_SceneCamera = std::make_unique<PlayerCamera>();
  m_SceneCamera->Position.y = 0;
  m_SceneCamera->Position.z = 0;

  static auto defaultShader = ShaderManager::Get()->GetShader("SH_DefaultBasicBatch");

  Ref<MeshSource>
      boxModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/models/box.gltf");

  auto redMat = Material::CreateMaterial("M_BoxRed", defaultShader);
  auto matProps = MaterialProperties{
      .ambientColor = glm::vec3(),
      .diffuseColor = glm::vec3(1.0f, 0.0, 0),
      .specularColor = glm::vec3(1),
      .shininess = 64};
  redMat->Set("uMaterial", matProps);
  redMat->Bake();

  auto greenMat = Material::CreateMaterial("M_BoxGreen", defaultShader);
  auto matPropsGreen = MaterialProperties{
      .ambientColor = glm::vec3(),
      .diffuseColor = glm::vec3(0.0f, 1.0, 0),
      .specularColor = glm::vec3(1),
      .shininess = 64};
  greenMat->Set("uMaterial", matPropsGreen);
  greenMat->Bake();

  Entity light = CreateEntity("DirectionalLight");
  light.GetComponent<TransformComponent>()->Translation = glm::vec3(30, 50, 0);
  light.AddComponent<DirectionalLightComponent>();

  Entity sampleEntity = CreateEntity("Floor");
  sampleEntity.GetComponent<TransformComponent>()->Translation = glm::vec3(0, 0, 0);
  sampleEntity.GetComponent<TransformComponent>()->Scale = glm::vec3(20, 1, 20);

  Entity sampleEntity2 = CreateEntity("Box");
  sampleEntity2.GetComponent<TransformComponent>()->Translation = glm::vec3(0, 5, 0);
  sampleEntity2.GetComponent<TransformComponent>()->Scale = glm::vec3(1);

  BuildMeshEntityHierarchy(sampleEntity, boxModel);
  BuildMeshEntityHierarchy(sampleEntity2, boxModel);

  auto childMesh = TryGetEntityWithUUID(sampleEntity.Children()[0]);
  childMesh.GetComponent<MeshComponent>()->Materials->SetMaterial(0, redMat);

  auto childMesh2 = TryGetEntityWithUUID(sampleEntity2.Children()[0]);
  childMesh2.GetComponent<MeshComponent>()->Materials->SetMaterial(0, greenMat);
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
  renderer->SetScene(this);

  static float aspectRatio = Application::Get()->GetWindowSize().x / Application::Get()->GetWindowSize().y;
  static auto projectionMatrix = glm::perspective(glm::radians(75.0f), aspectRatio, 0.10f, 2500.0f);
  renderer->BeginScene({m_SceneCamera->GetViewMatrix(), projectionMatrix, 2500, 0.10f});

  static flecs::query<TransformComponent, MeshComponent> drawNodeQuery = m_World.query<TransformComponent, MeshComponent>();
  drawNodeQuery.each([&](flecs::entity entity, TransformComponent& transform, MeshComponent& meshComponent) {
    Entity e = Entity(entity, this);
    Ref<MeshSource> meshSource = Rain::ResourceManager::GetMeshSource(meshComponent.MeshSourceId);
    glm::mat4 entityTransform = GetWorldSpaceTransformMatrix(e);

    renderer->SubmitMesh(meshSource, meshComponent.SubMeshId, meshComponent.Materials, entityTransform);
  });

  static flecs::query<TransformComponent, DirectionalLightComponent> lightsQuery = m_World.query<TransformComponent, DirectionalLightComponent>();
  lightsQuery.each([&](flecs::entity entity, TransformComponent& transform, DirectionalLightComponent& directionalLight) {
    SceneLightInfo.LightPos = transform.Translation;
  });
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

    nodeEntity.AddComponent<MeshComponent>((uint32_t)mesh->Id, (uint32_t)node->SubMeshId, materialId);
    nodeEntity.AddComponent<TransformComponent>();

    nodeEntity.Transform()->SetTransform(node->LocalTransform);
  }
}

glm::mat4 Scene::GetWorldSpaceTransformMatrix(Entity entity) {
  glm::mat4 transform(1.0f);

  Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());
  if (parent) {
    transform = GetWorldSpaceTransformMatrix(parent);
  }

  return transform * entity.GetComponent<TransformComponent>()->GetTransform();
}

void Scene::ScanKeyPress() {
  float speed = 1.1f;

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
