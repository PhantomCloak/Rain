#pragma once
#include <memory>
#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>

#include "physics/PhysicsBody.h"
#include "physics/PhysicsWheel.h"
#include "physics/PhysicTypes.h"
#include "scene/Entity.h"

class RainTrackedVehicle;
namespace WebEngine
{
  class PhysicsScene
  {
   public:
    PhysicsScene(glm::vec3 gravity);
    void Init(glm::vec3 gravity = glm::vec3(0, 9.8f, 0.0));
    void Cleanup();

    Ref<PhysicsBody> CreateBody(const Entity& entity);
    Ref<PhysicsWheel> CreateWheel(JPH::VehicleConstraintSettings& vehicleSettings, Entity entity);

    void PreUpdate(float dt);
    void Update(float dt);
    bool CastRay(const RayCastInfo* rayCastInfo, SceneQueryHit& outHit);
    static JPH::BodyInterface& GetBodyInterface(bool shouldLock = true);
    static const JPH::BodyLockInterface& GetBodyLockInterface();

    Ref<PhysicsBody> GetEntityBodyByID(const UUID& entityID) const;
    Ref<PhysicsBody> GetEntityBody(const Entity& entity) const { return GetEntityBodyByID(entity.GetUUID()); }
    void SynchronizeBodyTransform(PhysicsBody* body);
    static void MarkForSynchronization(PhysicsBody* body)
    {
      m_Instance->m_BodiesScheduledForSync.push_back(body);
    }

    void SynchronizePendingBodyTransforms()
    {
      for (auto body : m_BodiesScheduledForSync)
      {
        SynchronizeBodyTransform(body);
      }

      m_BodiesScheduledForSync.clear();
    }
    static PhysicsScene* m_Instance;
    std::unique_ptr<JPH::PhysicsSystem> m_PhysicsSystem;

   protected:
    friend class RainTrackedVehicle;

   private:
    std::unique_ptr<JPH::TempAllocatorImpl> m_TempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_JobSystem;

    std::unique_ptr<BPLayerInterfaceImpl> m_BroadPhaseLayerInterface;
    std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> m_ObjectVsBroadPhaseLayerFilter;
    std::unique_ptr<ObjectLayerPairFilterImpl> m_ObjectLayerPairFilter;

    std::unordered_map<UUID, Ref<PhysicsBody>> m_RigidBodies;
    std::unordered_map<UUID, Ref<JPH::Wheel>> m_WheelColliders;

    std::vector<PhysicsBody*> m_BodiesScheduledForSync;
  };
}  // namespace WebEngine
