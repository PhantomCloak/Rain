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
#include <Tracy.hpp>

Scene::Scene(std::string sceneName)
    : m_Name(sceneName) {}

Entity Scene::CreateEntity(std::string name) {
  return CreateChildEntity({}, name);
}

Ref<Material> redMat;
MaterialProperties redMatProps;

void Scene::Init() {
  m_SceneCamera = std::make_unique<PlayerCamera>();
  m_SceneCamera->Position.y = 0;
  m_SceneCamera->Position.z = 0;

  static auto defaultShader = ShaderManager::Get()->GetShader("SH_DefaultBasicBatch");

//  Ref<Texture> test = Rain::ResourceManager::LoadCubeTexture("T3D_Skybox",
//                                                             {"textures/cubemap-posX.png",
//                                                              "textures/cubemap-negX.png",
//                                                              "textures/cubemap-posY.png",
//                                                              "textures/cubemap-negY.png",
//                                                              "textures/cubemap-posZ.png",
//                                                              "textures/cubemap-posZ.png"});
//
  Ref<MeshSource>
      model = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/models/Helment/untitled.gltf");

  // sphere
  redMat = Material::CreateMaterial("M_BoxRed", defaultShader);
  redMatProps = MaterialProperties{
      .Metallic = 0.2f,
      .Roughness = 0.1f,
      .Ao = 0.3f};

  redMat->Set("uMaterial", redMatProps);
  redMat->Bake();

  Entity light = CreateEntity("DirectionalLight");
  light.GetComponent<TransformComponent>()->Translation = glm::vec3(10, 15, 5);
  light.AddComponent<DirectionalLightComponent>();

  Entity sampleEntity = CreateEntity("Test");
  sampleEntity.GetComponent<TransformComponent>()->Translation = glm::vec3(0, 0, -10);
  sampleEntity.GetComponent<TransformComponent>()->Scale = glm::vec3(1);

  BuildMeshEntityHierarchy(sampleEntity, model);

  // TryGetEntityWithUUID(sampleEntity.GetChild(0)).GetComponent<MeshComponent>()->Materials->SetMaterial(0, redMat);
  //  TryGetEntityWithUUID(wallOne.GetChild(0)).GetComponent<MeshComponent>()->Materials->SetMaterial(0, greenMat);
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
	RN_PROFILE_FUNC;
  renderer->SetScene(this);

  static float aspectRatio = Application::Get()->GetWindowSize().x / Application::Get()->GetWindowSize().y;
  static auto projectionMatrix = glm::perspective(glm::radians(45.0f), aspectRatio, 0.10f, 2500.0f);
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

  ImGui::Begin("Scene Settings");

  static float s_metallic = 0.1f;
  if (ImGui::SliderFloat("Metallic", &s_metallic, 0, 1)) {
    redMatProps.Metallic = s_metallic;
    redMat->Set("uMaterial", redMatProps);
  }

  ImGui::Spacing();
  static float s_roughness = 0.1f;
  if (ImGui::SliderFloat("Roughness", &s_roughness, 0, 1)) {
    redMatProps.Roughness = s_roughness;
    redMat->Set("uMaterial", redMatProps);
  }
  ImGui::Spacing();
  static float s_ao = 0.1f;
  if (ImGui::SliderFloat("Ao", &s_ao, 0, 1)) {
    redMatProps.Ao = s_ao;
    redMat->Set("uMaterial", redMatProps);
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

    nodeEntity.AddComponent<MeshComponent>((uint32_t)mesh->Id, (uint32_t)node->SubMeshId, materialId);
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
  float speed = 0.05f;

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
