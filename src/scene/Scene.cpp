#include "Scene.h"
#include "Components.h"
#include "core/Assert.h"
#include "render/Model.h"
#include "render/RenderQueue.h"
#include "render/ResourceManager.h"

Scene::Scene(std::string sceneName)
    : m_Name(sceneName) {}

Entity Scene::CreateEntity(std::string name) {
  return CreateChildEntity({}, name);
}

Entity Scene::CreateChildEntity(Entity parent, std::string name) {
  static UUID nextComponentId = 1000;  // TODO: remove it

  auto entity = Entity(m_World.entity(name.c_str()), this);
  entity.AddComponent<TransformComponent>();
  if (!name.empty()) {
    TagComponent* tagComponent = entity.AddComponent<TagComponent>();
    tagComponent->Tag = name;
  }
  entity.AddComponent<RelationshipComponent>();
  IDComponent* idComponent = entity.AddComponent<IDComponent>();
  idComponent->ID = nextComponentId;

  if (parent) {
    entity.SetParent(parent);
  }

  m_EntityMap[nextComponentId++] = entity;

  return entity;
}

void Scene::SubmitMesh(Ref<MeshSource> meshSource, uint32_t submeshIndex, uint32_t materialIndex, glm::mat4& transform) {

	MeshKey meshKey = { meshSource->Id, materialIndex, submeshIndex };

	auto& drawCommand = m_DrawList[meshKey];

	drawCommand.Mesh = meshSource;
	drawCommand.SubmeshIndex = submeshIndex;
	drawCommand.MaterialIndex = materialIndex;
	drawCommand.InstanceCount++;
}

void Scene::OnRender() {
  static flecs::query<TransformComponent, MeshComponent> drawNodeQuery = m_World.query<TransformComponent, MeshComponent>();

  drawNodeQuery.each([this](flecs::iter& it, size_t i, TransformComponent* transform, MeshComponent* meshComponent) {

    Entity e = Entity(it.entity(i), this);

		Ref<MeshSource> meshSource = Rain::ResourceManager::GetMeshSource(meshComponent->MeshSourceId);

		glm::mat4 entityTransform = GetWorldSpaceTransformMatrix(e);

		SubmitMesh(meshSource, meshComponent->MeshSourceId, meshComponent->MaterialId, entityTransform);
  });
}

Entity Scene::TryGetEntityWithUUID(UUID id) const {
  if (const auto iter = m_EntityMap.find(id); iter != m_EntityMap.end()) {
    return iter->second;
  }
  return {};
}

void Scene::BuildMeshEntityHierarchy(Entity parent, Ref<MeshSource> mesh) {
  const std::vector<Ref<MeshNode>> nodes = mesh->GetNodes();

  RenderQueue::IndexMeshSource(mesh);

  // TODO: make it more elegant
  uint32_t ctx = 0;
  for (const Ref<MeshNode> node : mesh->GetNodes()) {
    if (node->IsRoot()) {
      continue;
    }
    Entity nodeEntity = CreateChildEntity(parent, node->Name);
    nodeEntity.AddComponent<MeshComponent>((uint32_t)mesh->Id, (uint32_t)node->SubMeshId, ctx);
    nodeEntity.AddComponent<TransformComponent>();
    ctx++;
  }
}

Entity::operator bool() const {
  return (m_Entity != flecs::entity::null()) && m_Scene && m_Scene->m_World.is_valid(m_Entity);
}

Entity Entity::GetParent() const {
  return m_Scene->TryGetEntityWithUUID(GetParentUUID());
}

glm::mat4 Scene::GetWorldSpaceTransformMatrix(Entity entity) {
  glm::mat4 transform(1.0f);

  Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());
  if (parent) {
    transform = GetWorldSpaceTransformMatrix(parent);
  }

  return transform * entity.GetComponent<TransformComponent>()->GetTransform();
}
