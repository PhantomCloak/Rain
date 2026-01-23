#include "PhysicsScene.h"
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRenderer.h>
#endif
#include "PhysicUtils.h"
#include "Physics/Body/BodyCreationSettings.h"
#include "Physics/Body/BodyLockMulti.h"
#include "Physics/Collision/CastResult.h"
#include "Physics/Collision/CollisionCollectorImpl.h"
#include "Physics/Collision/RayCast.h"
#include "Physics/Collision/Shape/BoxShape.h"
#include "Physics/Collision/Shape/CylinderShape.h"
#include "Physics/Collision/Shape/OffsetCenterOfMassShape.h"
#include "Physics/Constraints/HingeConstraint.h"
#include "Physics/Vehicle/TrackedVehicleController.h"
#include "Physics/Vehicle/VehicleConstraint.h"
#include "Physics/Vehicle/VehicleTrack.h"
#include "core/KeyCode.h"
#include "io/keyboard.h"
#include "render/Render2D.h"
#include "scene/Scene.h"
#include "physics/RainTrackedVehicle.h"
namespace Rain
{
  PhysicsScene* PhysicsScene::m_Instance;
  RainTrackedVehicle* veh;
  PhysicsScene::PhysicsScene(glm::vec3 gravity)
  {
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    m_TempAllocator = new JPH::TempAllocatorImpl(64 * 1024 * 1024);
    m_JobSystem = new JPH::JobSystemThreadPool(2048, 8, JPH::thread::hardware_concurrency() - 1);

    m_BroadPhaseLayerInterface = new BPLayerInterfaceImpl();
    m_ObjectVsBroadPhaseLayerFilter = new ObjectVsBroadPhaseLayerFilterImpl();
    m_ObjectLayerPairFilter = new ObjectLayerPairFilterImpl();

    m_PhysicsSystem = new JPH::PhysicsSystem();
    m_PhysicsSystem->Init(
        1024 * 16,  // cMaxBodies
        0,          // cNumBodyMutexes
        1024 * 16,  // cMaxBodyPairs
        1024 * 16,  // cMaxContactConstraints
        *m_BroadPhaseLayerInterface,
        *m_ObjectVsBroadPhaseLayerFilter,
        *m_ObjectLayerPairFilter);
    m_PhysicsSystem->SetGravity(PhysicsUtils::ToJoltVector(gravity));
    m_Instance = this;

#ifndef __EMSCRIPTEN__
    RenderDebug::Init();
#endif

    //// Create turret
    // JPH::RVec3 turret_position = body_position + JPH::Vec3(0, half_vehicle_height + half_turret_height, 0);
    // JPH::BodyCreationSettings turret_body_setings(new JPH::BoxShape(JPH::Vec3(half_turret_width, half_turret_height, half_turret_length)), turret_position, JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, Layers::MOVING);
    // turret_body_setings.mCollisionGroup.SetGroupFilter(filter);
    // turret_body_setings.mCollisionGroup.SetGroupID(0);
    // turret_body_setings.mCollisionGroup.SetSubGroupID(0);
    // turret_body_setings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    // turret_body_setings.mMassPropertiesOverride.mMass = 2000.0f;
    // auto mTurretBody = m_PhysicsSystem->GetBodyInterface().CreateBody(turret_body_setings);
    // m_PhysicsSystem->GetBodyInterface().AddBody(mTurretBody->GetID(), JPH::EActivation::Activate);

    //// Attach turret to body
    // JPH::HingeConstraintSettings turret_hinge;
    // turret_hinge.mPoint1 = turret_hinge.mPoint2 = body_position + JPH::Vec3(0, half_vehicle_height, 0);
    // turret_hinge.mHingeAxis1 = turret_hinge.mHingeAxis2 = JPH::Vec3::sAxisY();
    // turret_hinge.mNormalAxis1 = turret_hinge.mNormalAxis2 = JPH::Vec3::sAxisZ();
    // turret_hinge.mMotorSettings = JPH::MotorSettings(0.5f, 1.0f);
    // auto mTurretHinge = static_cast<JPH::HingeConstraint*>(turret_hinge.Create(*mTankBody, *mTurretBody));
    // mTurretHinge->SetMotorState(JPH::EMotorState::Position);
    // m_PhysicsSystem->AddConstraint(mTurretHinge);

    //// Create barrel
    // JPH::RVec3 barrel_position = turret_position + JPH::Vec3(0, 0, half_turret_length + half_barrel_length - barrel_rotation_offset);
    // JPH::BodyCreationSettings barrel_body_setings(new JPH::CylinderShape(half_barrel_length, barrel_radius), barrel_position, JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 0.5f * JPH::JPH_PI), JPH::EMotionType::Dynamic, Layers::MOVING);
    // barrel_body_setings.mCollisionGroup.SetGroupFilter(filter);
    // barrel_body_setings.mCollisionGroup.SetGroupID(0);
    // barrel_body_setings.mCollisionGroup.SetSubGroupID(0);
    // barrel_body_setings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    // barrel_body_setings.mMassPropertiesOverride.mMass = 200.0f;
    // auto mBarrelBody = m_PhysicsSystem->GetBodyInterface().CreateBody(barrel_body_setings);
    // m_PhysicsSystem->GetBodyInterface().AddBody(mBarrelBody->GetID(), JPH::EActivation::Activate);

    //// Attach barrel to turret
    // JPH::HingeConstraintSettings barrel_hinge;
    // barrel_hinge.mPoint1 = barrel_hinge.mPoint2 = barrel_position - JPH::Vec3(0, 0, half_barrel_length);
    // barrel_hinge.mHingeAxis1 = barrel_hinge.mHingeAxis2 = -JPH::Vec3::sAxisX();
    // barrel_hinge.mNormalAxis1 = barrel_hinge.mNormalAxis2 = JPH::Vec3::sAxisZ();
    // barrel_hinge.mLimitsMin = JPH::DegreesToRadians(-10.0f);
    // barrel_hinge.mLimitsMax = JPH::DegreesToRadians(40.0f);
    // barrel_hinge.mMotorSettings = JPH::MotorSettings(10.0f, 1.0f);
    // auto mBarrelHinge = static_cast<JPH::HingeConstraint*>(barrel_hinge.Create(*mTurretBody, *mBarrelBody));
    // mBarrelHinge->SetMotorState(JPH::EMotorState::Position);
    // m_PhysicsSystem->AddConstraint(mBarrelHinge);
    // veh = new RainTrackedVehicle();
    // veh->CreateVehicle(this);
  }

  Ref<PhysicsBody> PhysicsScene::GetEntityBodyByID(UUID entityID) const
  {
    if (auto iter = m_RigidBodies.find(entityID); iter != m_RigidBodies.end())
    {
      return iter->second;
    }

    return nullptr;
  }

  JPH::BodyInterface& PhysicsScene::GetBodyInterface(bool shouldLock)
  {
    return shouldLock ? m_Instance->m_PhysicsSystem->GetBodyInterface() : m_Instance->m_PhysicsSystem->GetBodyInterfaceNoLock();
  }

  const JPH::BodyLockInterface& PhysicsScene::GetBodyLockInterface()
  {
    return m_Instance->m_PhysicsSystem->GetBodyLockInterface();
  }

  void PhysicsScene::SynchronizeBodyTransform(PhysicsBody* body)
  {
    JPH::BodyLockRead bodyLock(PhysicsScene::GetBodyLockInterface(), body->m_BodyID);
    const JPH::Body& bodyRef = bodyLock.GetBody();

    Entity entity = Scene::Instance->TryGetEntityWithUUID(bodyRef.GetUserData());

    TransformComponent& transformComponent = entity.GetComponent<TransformComponent>();
    glm::vec3 scale = transformComponent.Scale;
    transformComponent.Translation = PhysicsUtils::FromJoltVector(bodyRef.GetPosition());
    transformComponent.SetRotation(PhysicsUtils::FromJoltQuat(bodyRef.GetRotation()));

    Scene::Instance->ConvertToLocalSpace(entity);
    transformComponent.Scale = scale;
  }

  void PhysicsScene::PreUpdate(float dt)
  {
    for (auto& [entityID, body] : m_RigidBodies)
    {
      if (body->IsKinematic())
      {
        Entity entity = Scene::Instance->TryGetEntityWithUUID(entityID);
        auto tc = Scene::Instance->GetWorldSpaceTransform(entity);

        glm::vec3 currentBodyTranslation = body->GetTranslation();
        glm::quat currentBodyRotation = body->GetRotation();
        if (glm::any(glm::epsilonNotEqual(currentBodyTranslation, tc.Translation, 0.00001f)) || glm::any(glm::epsilonNotEqual(currentBodyRotation, tc.GetRotation(), 0.00001f)))
        {
          if (body->IsSleeping())
          {
            body->SetSleepState(false);
          }

          glm::vec3 targetTranslation = tc.Translation;
          glm::quat targetRotation = tc.GetRotation();
          if (glm::dot(currentBodyRotation, targetRotation) < 0.0f)
          {
            targetRotation = -targetRotation;
          }

          if (glm::any(glm::epsilonNotEqual(currentBodyRotation, targetRotation, 0.000001f)))
          {
            glm::vec3 currentBodyEuler = glm::eulerAngles(currentBodyRotation);
            glm::vec3 targetBodyEuler = glm::eulerAngles(tc.GetRotation());

            glm::quat rotation = tc.GetRotation() * glm::conjugate(currentBodyRotation);
            glm::vec3 rotationEuler = glm::eulerAngles(rotation);
          }

          body->MoveKinematic(tc.Translation, targetRotation, dt);
        }
      }
    }
  }

  void PhysicsScene::Update(float dt)
  {
    // Input
    // veh->Update(this);

    return;
    // m_PhysicsSystem->DrawBodies(JPH::BodyManager::DrawSettings(), JPH::DebugRenderer::sInstance);

    // PreUpdate(dt);
    // SynchronizePendingBodyTransforms();
    // m_PhysicsSystem->Update(dt, 1, m_TempAllocator, m_JobSystem);

    // const auto& bodyLockInterface = m_PhysicsSystem->GetBodyLockInterface();
    // JPH::BodyIDVector activeBodies;
    // m_PhysicsSystem->GetActiveBodies(JPH::EBodyType::RigidBody, activeBodies);
    // JPH::BodyLockMultiWrite activeBodiesLock(bodyLockInterface, activeBodies.data(), static_cast<int32_t>(activeBodies.size()));

    // for (int32_t i = 0; i < activeBodies.size(); i++)
    //{
    //   JPH::Body* body = activeBodiesLock.GetBody(i);
    //   // The position of kinematic rigid bodies is synced _before_ the physics simulation by setting its velocity such that
    //   // the simulation will move it to the game world position.  This gives a better collision response than synching the
    //   // position here.
    //   if (body == nullptr || body->IsKinematic())
    //   {
    //     continue;
    //   }

    //  // Apply air resistance
    //  // body->ApplyBuoyancyImpulse(s_AirPlane, 0.075f, 0.15f, 0.01f, JPH::Vec3::sZero(), m_JoltSystem->GetGravity(), m_FixedTimeStep);

    //  // Synchronize the transform
    //  Entity entity = Scene::Instance->TryGetEntityWithUUID(body->GetUserData());

    //  if (!entity)
    //  {
    //    continue;
    //  }

    //  auto rigidBody = m_RigidBodies.at(entity.GetUUID());

    //  TransformComponent& transformComponent = entity.GetComponent<TransformComponent>();
    //  glm::vec3 scale = transformComponent.Scale;
    //  transformComponent.Translation = PhysicsUtils::FromJoltVector(body->GetPosition());

    //  transformComponent.SetRotation(PhysicsUtils::FromJoltQuat(body->GetRotation()));

    //  Scene::Instance->ConvertToLocalSpace(entity);
    //  transformComponent.Scale = scale;
    //}
  }

  Ref<PhysicsBody> PhysicsScene::CreateBody(Entity entity)
  {
    // if (!entity.HasAny<CompoundColliderComponent, BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent, MeshColliderComponent>()) {
    //   // This can happen as a result of a user trying to create a RigidBodyComponent from C# script on an entity that does
    //   // not have a collider.
    //   // Tell them what went wrong, rather than silently doing nothing.
    //   HZ_CONSOLE_LOG_ERROR("Entity does not have a collider component!");
    //   return nullptr;
    // }

    if (auto existingBody = GetEntityBody(entity))
    {
      return existingBody;
    }

    JPH::BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();
    Ref<PhysicsBody> rigidBody = CreateRef<PhysicsBody>(bodyInterface, entity);

    if (rigidBody->GetBodyId().IsInvalid())
    {
      return nullptr;
    }

    bodyInterface.AddBody(rigidBody->GetBodyId(), JPH::EActivation::Activate);

    m_RigidBodies[entity.GetUUID()] = rigidBody;
    return rigidBody;
  }

  Ref<PhysicsWheel> PhysicsScene::CreateWheel(JPH::VehicleConstraintSettings& vehicleSettings, Entity entity)
  {
    return CreateRef<PhysicsWheel>(vehicleSettings, entity);
  }

  bool PhysicsScene::CastRay(const RayCastInfo* rayCastInfo, SceneQueryHit& outHit)
  {
    outHit.Clear();

    JPH::RayCast ray;
    ray.mOrigin = PhysicsUtils::ToJoltVector(rayCastInfo->Origin);
    ray.mDirection = PhysicsUtils::ToJoltVector(glm::normalize(rayCastInfo->Direction)) * rayCastInfo->MaxDistance;

    JPH::ClosestHitCollisionCollector<JPH::CastRayCollector> hitCollector;
    JPH::RayCastSettings rayCastSettings;
    m_PhysicsSystem->GetNarrowPhaseQuery().CastRay(JPH::RRayCast(ray), rayCastSettings, hitCollector, {}, {}, {});

    JPH::BodyLockRead bodyLock(m_PhysicsSystem->GetBodyLockInterface(), hitCollector.mHit.mBodyID);
    if (bodyLock.Succeeded())
    {
      const JPH::Body& body = bodyLock.GetBody();

      JPH::Vec3 hitPosition = ray.GetPointOnRay(hitCollector.mHit.mFraction);

      outHit.HitEntity = body.GetUserData();
      outHit.Position = PhysicsUtils::FromJoltVector(hitPosition);
      outHit.Normal = PhysicsUtils::FromJoltVector(body.GetWorldSpaceSurfaceNormal(hitCollector.mHit.mSubShapeID2, hitPosition));
      outHit.Distance = glm::distance(rayCastInfo->Origin, outHit.Position);

      return true;
    }

    return false;
  }
}  // namespace Rain
