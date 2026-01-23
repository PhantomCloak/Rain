#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include "Physics/Vehicle/VehicleConstraint.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsWheel.h"
#include "physics/PhysicTypes.h"
#include "scene/Entity.h"

class RainTrackedVehicle;
namespace Rain
{
  class PhysicsScene
  {
   public:
    PhysicsScene(glm::vec3 gravity);

    Ref<PhysicsBody> CreateBody(Entity entity);
    Ref<PhysicsWheel> CreateWheel(JPH::VehicleConstraintSettings& vehicleSettings, Entity entity);

    void PreUpdate(float dt);
    void Update(float dt);
    bool CastRay(const RayCastInfo* rayCastInfo, SceneQueryHit& outHit);
    static JPH::BodyInterface& GetBodyInterface(bool shouldLock = true);
    static const JPH::BodyLockInterface& GetBodyLockInterface();

    Ref<PhysicsBody> GetEntityBodyByID(UUID entityID) const;
    Ref<PhysicsBody> GetEntityBody(Entity entity) const { return GetEntityBodyByID(entity.GetUUID()); }
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
    JPH::PhysicsSystem* m_PhysicsSystem = nullptr;

   protected:
    friend class RainTrackedVehicle;

   private:
    JPH::TempAllocatorImpl* m_TempAllocator = nullptr;
    JPH::JobSystemThreadPool* m_JobSystem = nullptr;

    BPLayerInterfaceImpl* m_BroadPhaseLayerInterface = nullptr;
    ObjectVsBroadPhaseLayerFilterImpl* m_ObjectVsBroadPhaseLayerFilter = nullptr;
    ObjectLayerPairFilterImpl* m_ObjectLayerPairFilter = nullptr;

    std::unordered_map<UUID, Ref<PhysicsBody>> m_RigidBodies;
    std::unordered_map<UUID, Ref<JPH::Wheel>> m_WheelColliders;

    std::vector<PhysicsBody*> m_BodiesScheduledForSync;
    JPH::Body* mTankBody;
    JPH::VehicleConstraint* mVehicleConstraint;
    float mForward = 0.0f;
    float mPreviousForward = 1.0f;
    float mLeftRatio = 0.0f;
    float mRightRatio = 0.0f;
    float mBrake = 0.0f;
    float mTurretHeading = 0.0f;
    float mBarrelPitch = 0.0f;
    bool mFire = false;
  };
}  // namespace Rain
