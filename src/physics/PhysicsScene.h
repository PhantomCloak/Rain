#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include "PhysicsBody.h"
#include "physics/PhysicTypes.h"
#include "scene/Entity.h"

namespace Rain {
  class PhysicsScene {
   public:
    PhysicsScene(glm::vec3 gravity);

    Ref<PhysicsBody> CreateBody(Entity entity);
    void PreUpdate(float dt);
    void Update(float dt);
    bool CastRay(const RayCastInfo* rayCastInfo, SceneQueryHit& outHit);
    static JPH::BodyInterface& GetBodyInterface(bool shouldLock = true);
    static const JPH::BodyLockInterface& GetBodyLockInterface();

    Ref<PhysicsBody> GetEntityBodyByID(UUID entityID) const;
    Ref<PhysicsBody> GetEntityBody(Entity entity) const { return GetEntityBodyByID(entity.GetUUID()); }
    void SynchronizeBodyTransform(PhysicsBody* body);
    static void MarkForSynchronization(PhysicsBody* body) {
      m_Instance->m_BodiesScheduledForSync.push_back(body);
    }

    void SynchronizePendingBodyTransforms() {
      // Synchronize bodies that requested it explicitly
      for (auto body : m_BodiesScheduledForSync) {
        SynchronizeBodyTransform(body);
      }

      m_BodiesScheduledForSync.clear();
    }

   private:
    static PhysicsScene* m_Instance;

    JPH::PhysicsSystem* m_PhysicsSystem = nullptr;
    JPH::TempAllocatorImpl* m_TempAllocator = nullptr;
    JPH::JobSystemThreadPool* m_JobSystem = nullptr;

    BPLayerInterfaceImpl* m_BroadPhaseLayerInterface = nullptr;
    ObjectVsBroadPhaseLayerFilterImpl* m_ObjectVsBroadPhaseLayerFilter = nullptr;
    ObjectLayerPairFilterImpl* m_ObjectLayerPairFilter = nullptr;

    std::unordered_map<UUID, Ref<PhysicsBody>> m_RigidBodies;
    std::vector<PhysicsBody*> m_BodiesScheduledForSync;
  };
}  // namespace Rain
