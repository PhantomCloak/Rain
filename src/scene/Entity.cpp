#include "Entity.h"
#include "Scene.h"


Entity::operator bool() const {
  return (m_Entity != flecs::entity::null()) && m_Scene && m_Scene->m_World.is_valid(m_Entity);
}

Entity Entity::GetParent() const {
  return m_Scene->TryGetEntityWithUUID(GetParentUUID());
}
