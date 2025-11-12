#include "PhysicsBody.h"
#include "Physics/Collision/Shape/CylinderShape.h"
#include "physics/PhysicUtils.h"
#include "scene/Scene.h"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsScene.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>

namespace Rain
{
  PhysicsBody::PhysicsBody(JPH::BodyInterface& bodyInterface, Entity entity)
  {
    const RigidBodyComponent& rigidBodyComponent = entity.GetComponent<RigidBodyComponent>();
    m_Entity = entity;

    switch (rigidBodyComponent.BodyType)
    {
      case EBodyType::Static:
      {
        CreateStaticBody(bodyInterface);
        break;
      }
      case EBodyType::Dynamic:
      {
        CreateDynamicBody(bodyInterface);
        break;
      }
    }
  }

  glm::vec3 PhysicsBody::GetTranslation() const
  {
    return PhysicsUtils::FromJoltVector(PhysicsScene::GetBodyInterface().GetPosition(m_BodyID));
  }

  glm::quat PhysicsBody::GetRotation() const
  {
    return PhysicsUtils::FromJoltQuat(PhysicsScene::GetBodyInterface().GetRotation(m_BodyID));
  }

  glm::vec3 PhysicsBody::GetLinearVelocity() const
  {
    return PhysicsUtils::FromJoltVector(PhysicsScene::GetBodyInterface().GetLinearVelocity(m_BodyID));
  }

  bool PhysicsBody::IsKinematic() const
  {
    return PhysicsScene::GetBodyInterface().GetMotionType(m_BodyID) == JPH::EMotionType::Kinematic;
  }

  bool PhysicsBody::IsSleeping() const
  {
    JPH::BodyLockRead lock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
    return !lock.GetBody().IsActive();
  }

  void PhysicsBody::Activate()
  {
    return PhysicsScene::GetBodyInterface().ActivateBody(m_BodyID);
  }

  void PhysicsBody::MoveKinematic(const glm::vec3& targetPosition, const glm::quat& targetRotation, float deltaSeconds)
  {
    JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);

    JPH::Body& body = bodyLock.GetBody();

    if (body.GetMotionType() != JPH::EMotionType::Kinematic)
    {
      return;
    }

    body.MoveKinematic(PhysicsUtils::ToJoltVector(targetPosition), PhysicsUtils::ToJoltQuat(targetRotation), deltaSeconds);
  }

  void PhysicsBody::SetSleepState(bool inSleep)
  {
    JPH::BodyLockWrite lock(PhysicsScene::GetBodyLockInterface(), m_BodyID);

    if (!lock.Succeeded())
    {
      return;
    }

    auto& body = lock.GetBody();

    if (inSleep)
    {
      PhysicsScene::GetBodyInterface(false).DeactivateBody(m_BodyID);
    }
    else if (body.IsInBroadPhase())
    {
      PhysicsScene::GetBodyInterface(false).ActivateBody(m_BodyID);
    }
  }

  void PhysicsBody::Update()
  {
    PhysicsScene::MarkForSynchronization(this);
  }

  void PhysicsBody::SetTranslation(const glm::vec3& translation)
  {
    JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
    JPH::Body& body = bodyLock.GetBody();

    if (!body.IsStatic())
    {
      return;
    }

    PhysicsScene::GetBodyInterface(false).SetPosition(m_BodyID, PhysicsUtils::ToJoltVector(translation), JPH::EActivation::DontActivate);

    PhysicsScene::MarkForSynchronization(this);
  }

  void PhysicsBody::CreateCollisionShapesForEntity(Entity entity)
  {
    // if (entity.HasComponent<BoxColliderComponent>()) {
    //   BoxColliderComponent& collider = entity.GetComponent<BoxColliderComponent>();

    //  if (collider.Offset != glm::vec3(0.0)) {
    //    m_Shape = JPH::OffsetCenterOfMassShapeSettings(PhysicsUtils::ToJoltVector(collider.Offset), new JPH::BoxShape(PhysicsUtils::ToJoltVector(collider.Size))).Create().Get();
    //  } else {
    //    m_Shape = new JPH::BoxShape(PhysicsUtils::ToJoltVector(collider.Size));
    //  }
    //} else if (entity.HasComponent<CylinderCollider>()) {
    //  CylinderCollider& collider = entity.GetComponent<CylinderCollider>();
    //  m_Shape = new JPH::CylinderShape(collider.Size.x, collider.Size.y);
    //}
  }

  void PhysicsBody::CreateDynamicBody(JPH::BodyInterface& bodyInterface)
  {
    auto worldTransform = Scene::Instance->GetWorldSpaceTransform(m_Entity);
    const RigidBodyComponent& rigidBodyComponent = m_Entity.GetComponent<RigidBodyComponent>();

    CreateCollisionShapesForEntity(m_Entity);

    JPH::BodyCreationSettings bodySettings(
        m_Shape,
        PhysicsUtils::ToJoltVector(worldTransform.Translation),
        PhysicsUtils::ToJoltQuat(worldTransform.Rotation),
        JPH::EMotionType::Dynamic,
        Layers::MOVING);

    bodySettings.mAllowSleeping = false;
    bodySettings.mLinearDamping = rigidBodyComponent.LinearDrag;
    bodySettings.mAngularDamping = rigidBodyComponent.AngularDrag;
    // bodySettings.mGravityFactor = 1.0f;
    //  bodySettings.mLinearVelocity = PhysicsUtils::ToJoltVector(rigidBodyComponent->InitialLinearVelocity);
    //  bodySettings.mAngularVelocity = PhysicsUtils::ToJoltVector(rigidBodyComponent->InitialAngularVelocity);
    //  bodySettings.mMaxLinearVelocity = rigidBodyComponent->MaxLinearVelocity;
    //  bodySettings.mMaxAngularVelocity = rigidBodyComponent->MaxAngularVelocity;

    JPH::GroupFilter* filter = new JPH::GroupFilterTable;
    bodySettings.mCollisionGroup.SetGroupFilter(filter);
    bodySettings.mCollisionGroup.SetGroupID(0);
    bodySettings.mCollisionGroup.SetSubGroupID(0);
    bodySettings.mMotionQuality = JPH::EMotionQuality::Discrete;
    // bodySettings.mAllowSleeping = true;

    if (rigidBodyComponent.Mass != 1.0f)
    {
      bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
      bodySettings.mMassPropertiesOverride.mMass = rigidBodyComponent.Mass;
    }

    JPH::Body* body = bodyInterface.CreateBody(bodySettings);
    if (body == nullptr)
    {
      return;
    }

    body->SetUserData(m_Entity.GetUUID());
    m_BodyID = body->GetID();
  }

  void PhysicsBody::CreateStaticBody(JPH::BodyInterface& bodyInterface)
  {
    auto worldTransform = Scene::Instance->GetWorldSpaceTransform(m_Entity);
    const auto& rigidBodyComponent = m_Entity.GetComponent<RigidBodyComponent>();

    CreateCollisionShapesForEntity(m_Entity);

    JPH::BodyCreationSettings bodySettings(
        m_Shape,
        PhysicsUtils::ToJoltVector(worldTransform.Translation),
        // PhysicsUtils::ToJoltQuat(glm::normalize(worldTransform.Rotation)),
        PhysicsUtils::ToJoltQuat(worldTransform.Rotation),
        JPH::EMotionType::Static,
        Layers::MOVING);

    bodySettings.mIsSensor = false;
    bodySettings.mAllowDynamicOrKinematic = true;
    bodySettings.mAllowSleeping = false;

    JPH::Body* body = bodyInterface.CreateBody(bodySettings);
    if (body == nullptr)
    {
      return;
    }

    body->SetUserData(m_Entity.GetUUID());
    m_BodyID = body->GetID();
  }
}  // namespace Rain
