#include "PhysicsBody.h"
#include "physics/PhysicUtils.h"
#include "scene/Scene.h"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsScene.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

namespace Rain {
  PhysicsBody::PhysicsBody(JPH::BodyInterface& bodyInterface, Entity entity) {
    const auto& rigidBodyComponent = entity.GetComponent<RigidBodyComponent>();
    m_Entity = entity;

    switch (rigidBodyComponent->BodyType) {
      case EBodyType::Static: {
        CreateStaticBody(bodyInterface);
        break;
      }
      case EBodyType::Dynamic: {
        CreateDynamicBody(bodyInterface);
        break;
      }
    }
  }

  glm::vec3 PhysicsBody::GetTranslation() const {
    return PhysicsUtils::FromJoltVector(PhysicsScene::GetBodyInterface().GetPosition(m_BodyID));
  }

  glm::quat PhysicsBody::GetRotation() const {
    return PhysicsUtils::FromJoltQuat(PhysicsScene::GetBodyInterface().GetRotation(m_BodyID));
  }

  bool PhysicsBody::IsKinematic() const {
    return PhysicsScene::GetBodyInterface().GetMotionType(m_BodyID) == JPH::EMotionType::Kinematic;
  }

  bool PhysicsBody::IsSleeping() const {
    JPH::BodyLockRead lock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
    return !lock.GetBody().IsActive();
  }

  void PhysicsBody::MoveKinematic(const glm::vec3& targetPosition, const glm::quat& targetRotation, float deltaSeconds) {
    JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);

    JPH::Body& body = bodyLock.GetBody();

    if (body.GetMotionType() != JPH::EMotionType::Kinematic) {
      return;
    }

    body.MoveKinematic(PhysicsUtils::ToJoltVector(targetPosition), PhysicsUtils::ToJoltQuat(targetRotation), deltaSeconds);
  }

  void PhysicsBody::SetSleepState(bool inSleep) {
    JPH::BodyLockWrite lock(PhysicsScene::GetBodyLockInterface(), m_BodyID);

    if (!lock.Succeeded()) {
      return;
    }

    auto& body = lock.GetBody();

    if (inSleep) {
      PhysicsScene::GetBodyInterface(false).DeactivateBody(m_BodyID);
    } else if (body.IsInBroadPhase()) {
      PhysicsScene::GetBodyInterface(false).ActivateBody(m_BodyID);
    }
  }

  void PhysicsBody::Update() {
    PhysicsScene::MarkForSynchronization(this);
  }

  void PhysicsBody::SetTranslation(const glm::vec3& translation) {
    JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
    JPH::Body& body = bodyLock.GetBody();

    if (!body.IsStatic()) {
      return;
    }

    PhysicsScene::GetBodyInterface(false).SetPosition(m_BodyID, PhysicsUtils::ToJoltVector(translation), JPH::EActivation::DontActivate);

    PhysicsScene::MarkForSynchronization(this);
  }

  void PhysicsBody::CreateDynamicBody(JPH::BodyInterface& bodyInterface) {
    auto worldTransform = Scene::Instance->GetWorldSpaceTransform(m_Entity);
    const auto& rigidBodyComponent = m_Entity.GetComponent<RigidBodyComponent>();

    JPH::BodyCreationSettings bodySettings(
        new JPH::BoxShape(PhysicsUtils::ToJoltVector(worldTransform.Scale)),
        PhysicsUtils::ToJoltVector(worldTransform.Translation),
        PhysicsUtils::ToJoltQuat(glm::normalize(worldTransform.Rotation)),
        JPH::EMotionType::Dynamic,
        5);

    bodySettings.mAllowSleeping = false;
    bodySettings.mLinearDamping = rigidBodyComponent->LinearDrag;
    bodySettings.mAngularDamping = rigidBodyComponent->AngularDrag;
    bodySettings.mGravityFactor = 1.0f;
    bodySettings.mLinearVelocity = PhysicsUtils::ToJoltVector(rigidBodyComponent->InitialLinearVelocity);
    bodySettings.mAngularVelocity = PhysicsUtils::ToJoltVector(rigidBodyComponent->InitialAngularVelocity);
    bodySettings.mMaxLinearVelocity = rigidBodyComponent->MaxLinearVelocity;
    bodySettings.mMaxAngularVelocity = rigidBodyComponent->MaxAngularVelocity;
    bodySettings.mMotionQuality = JPH::EMotionQuality::Discrete;
    bodySettings.mAllowSleeping = true;

    JPH::Body* body = bodyInterface.CreateBody(bodySettings);
    if (body == nullptr) {
      return;
    }

    body->SetUserData(m_Entity.GetUUID());
    m_BodyID = body->GetID();
  }

  void PhysicsBody::CreateStaticBody(JPH::BodyInterface& bodyInterface) {
    auto worldTransform = Scene::Instance->GetWorldSpaceTransform(m_Entity);
    const auto& rigidBodyComponent = m_Entity.GetComponent<RigidBodyComponent>();

    JPH::BodyCreationSettings bodySettings(
        new JPH::BoxShape(PhysicsUtils::ToJoltVector(worldTransform.Scale)),
        PhysicsUtils::ToJoltVector(worldTransform.Translation),
        PhysicsUtils::ToJoltQuat(glm::normalize(worldTransform.Rotation)),
        JPH::EMotionType::Static,
        5);

    bodySettings.mIsSensor = false;
    bodySettings.mAllowDynamicOrKinematic = true;
    bodySettings.mAllowSleeping = false;

    JPH::Body* body = bodyInterface.CreateBody(bodySettings);
    if (body == nullptr) {
      return;
    }

    body->SetUserData(m_Entity.GetUUID());
    m_BodyID = body->GetID();
  }
}  // namespace Rain
