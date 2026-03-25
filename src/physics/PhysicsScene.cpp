#include "PhysicsScene.h"
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRenderer.h>
#endif
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Vehicle/TrackedVehicleController.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/VehicleTrack.h>
#include "core/KeyCode.h"
#include "io/keyboard.h"
#include "PhysicUtils.h"
#include "render/Render2D.h"
#include "scene/Scene.h"

namespace WebEngine
{
  PhysicsScene* PhysicsScene::m_Instance;

  PhysicsScene::PhysicsScene(glm::vec3 gravity)
  {
    Init(gravity);
  }

  void PhysicsScene::Init(glm::vec3 gravity)
  {
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    m_TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(64 * 1024 * 1024);
    m_JobSystem = std::make_unique<JPH::JobSystemThreadPool>(2048, 8, JPH::thread::hardware_concurrency() - 1);

    m_BroadPhaseLayerInterface = std::make_unique<BPLayerInterfaceImpl>();
    m_ObjectVsBroadPhaseLayerFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
    m_ObjectLayerPairFilter = std::make_unique<ObjectLayerPairFilterImpl>();

    m_PhysicsSystem = std::make_unique<JPH::PhysicsSystem>();
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
    //RenderDebug::Init();
#endif
  }

  void PhysicsScene::Cleanup()
  {
    JPH::BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();
    for (auto& [id, body] : m_RigidBodies)
    {
      if (body && !body->GetBodyId().IsInvalid())
      {
        bodyInterface.RemoveBody(body->GetBodyId());
        bodyInterface.DestroyBody(body->GetBodyId());
      }
    }

    m_RigidBodies.clear();
    m_WheelColliders.clear();
    m_BodiesScheduledForSync.clear();

    m_PhysicsSystem.reset();
    m_JobSystem.reset();
    m_TempAllocator.reset();
    m_BroadPhaseLayerInterface.reset();
    m_ObjectVsBroadPhaseLayerFilter.reset();
    m_ObjectLayerPairFilter.reset();

    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    m_Instance = nullptr;
  }

  Ref<PhysicsBody> PhysicsScene::GetEntityBodyByID(const UUID& entityID) const
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
    // for (auto& [entityID, body] : m_RigidBodies)
    //{
    //   if (body->IsKinematic())
    //   {
    //     Entity entity = Scene::Instance->TryGetEntityWithUUID(entityID);
    //     auto tc = Scene::Instance->GetWorldSpaceTransform(entity);

    //    glm::vec3 currentBodyTranslation = body->GetTranslation();
    //    glm::quat currentBodyRotation = body->GetRotation();
    //    if (glm::any(glm::epsilonNotEqual(currentBodyTranslation, tc.Translation, 0.00001f)) || glm::any(glm::epsilonNotEqual(currentBodyRotation, tc.GetRotation(), 0.00001f)))
    //    {
    //      if (body->IsSleeping())
    //      {
    //        body->SetSleepState(false);
    //      }

    //      glm::vec3 targetTranslation = tc.Translation;
    //      glm::quat targetRotation = tc.GetRotation();
    //      if (glm::dot(currentBodyRotation, targetRotation) < 0.0f)
    //      {
    //        targetRotation = -targetRotation;
    //      }

    //      if (glm::any(glm::epsilonNotEqual(currentBodyRotation, targetRotation, 0.000001f)))
    //      {
    //        glm::vec3 currentBodyEuler = glm::eulerAngles(currentBodyRotation);
    //        glm::vec3 targetBodyEuler = glm::eulerAngles(tc.GetRotation());

    //        glm::quat rotation = tc.GetRotation() * glm::conjugate(currentBodyRotation);
    //        glm::vec3 rotationEuler = glm::eulerAngles(rotation);
    //      }

    //      body->MoveKinematic(tc.Translation, targetRotation, dt);
    //    }
    //  }
    //}
  }

  void PhysicsScene::Update(float dt)
  {
    SynchronizePendingBodyTransforms();

    if (m_TempAllocator == nullptr || m_JobSystem == nullptr)
    {
      return;
    }

    m_PhysicsSystem->Update(dt, 1, m_TempAllocator.get(), m_JobSystem.get());

#ifdef JPH_DEBUG_RENDERER
    //JPH::BodyManager::DrawSettings drawSettings;
    //drawSettings.mDrawShape = true;
    //drawSettings.mDrawShapeWireframe = true;
    //m_PhysicsSystem->DrawBodies(drawSettings, JPH::DebugRenderer::sInstance);
#endif

    const auto& bodyLockInterface = m_PhysicsSystem->GetBodyLockInterface();
    JPH::BodyIDVector activeBodies;
    m_PhysicsSystem->GetActiveBodies(JPH::EBodyType::RigidBody, activeBodies);
    JPH::BodyLockMultiWrite activeBodiesLock(bodyLockInterface, activeBodies.data(), static_cast<int32_t>(activeBodies.size()));

    for (int32_t i = 0; i < (int32_t)activeBodies.size(); i++)
    {
      JPH::Body* body = activeBodiesLock.GetBody(i);
      if (body == nullptr || body->IsKinematic())
      {
        continue;
      }

      Entity entity = Scene::Instance->TryGetEntityWithUUID(body->GetUserData());

      if (!entity)
      {
        continue;
      }

      TransformComponent& transformComponent = entity.GetComponent<TransformComponent>();
      glm::vec3 scale = transformComponent.Scale;
      transformComponent.Translation = PhysicsUtils::FromJoltVector(body->GetPosition());
      transformComponent.SetRotation(PhysicsUtils::FromJoltQuat(body->GetRotation()));

      Scene::Instance->ConvertToLocalSpace(entity);
      transformComponent.Scale = scale;
    }
  }

  Ref<PhysicsBody> PhysicsScene::CreateBody(const Entity& entity)
  {
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

    // JPH::RayCast ray;
    // ray.mOrigin = PhysicsUtils::ToJoltVector(rayCastInfo->Origin);
    // ray.mDirection = PhysicsUtils::ToJoltVector(glm::normalize(rayCastInfo->Direction)) * rayCastInfo->MaxDistance;

    // JPH::ClosestHitCollisionCollector<JPH::CastRayCollector> hitCollector;
    // JPH::RayCastSettings rayCastSettings;
    // m_PhysicsSystem->GetNarrowPhaseQuery().CastRay(JPH::RRayCast(ray), rayCastSettings, hitCollector, {}, {}, {});

    // JPH::BodyLockRead bodyLock(m_PhysicsSystem->GetBodyLockInterface(), hitCollector.mHit.mBodyID);
    // if (bodyLock.Succeeded())
    //{
    //   const JPH::Body& body = bodyLock.GetBody();

    //  JPH::Vec3 hitPosition = ray.GetPointOnRay(hitCollector.mHit.mFraction);

    //  outHit.HitEntity = body.GetUserData();
    //  outHit.Position = PhysicsUtils::FromJoltVector(hitPosition);
    //  outHit.Normal = PhysicsUtils::FromJoltVector(body.GetWorldSpaceSurfaceNormal(hitCollector.mHit.mSubShapeID2, hitPosition));
    //  outHit.Distance = glm::distance(rayCastInfo->Origin, outHit.Position);

    //  return true;
    //}

    return false;
  }
}  // namespace WebEngine
