#include "RainTrackedVehicle.h"
#include <Jolt.h>
#include <Physics/Vehicle/TrackedVehicleController.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Physics/Vehicle/VehicleConstraint.h>
#include "Physics/Body/BodyLockMulti.h"
#include "Physics/Constraints/HingeConstraint.h"
#include "physics/PhysicUtils.h"
#include "render/ResourceManager.h"
#include "scene/Scene.h"

#include "core/KeyCode.h"
#include "io/keyboard.h"

namespace Rain {

  void RainTrackedVehicle::CreateVehicle(PhysicsScene* Scene) {
    m_Props.VehicleHeight = 1.5f;
    m_Props.VehicleWidth = 3.4f;
    m_Props.VehicleLenght = 5.4f;

    const float half_vehicle_length = m_Props.VehicleLenght / 2;
    const float half_vehicle_width = m_Props.VehicleWidth / 2;
    const float half_vehicle_height = m_Props.VehicleHeight / 2;
    const float suspension_frequency = 1.0f;

    auto tankBodyModel = ResourceManager::LoadMeshSource(RESOURCE_DIR "/tank_tiger/tank_body.gltf");
    auto boxModel = ResourceManager::LoadMeshSource(RESOURCE_DIR "/box.gltf");
    auto wheelModel = ResourceManager::LoadMeshSource(RESOURCE_DIR "/cylinder.gltf");

    m_VehicleEntity = Scene::Instance->CreateEntity("TankBody");
    // m_VehicleEntity.GetComponent<TransformComponent>()->Translation = glm::vec3(0, 2, 0);
    // m_VehicleEntity.GetComponent<TransformComponent>()->Scale = glm::vec3(half_vehicle_width, half_vehicle_height, half_vehicle_length);

    const auto vehicleRigidbody = m_VehicleEntity.AddComponent<RigidBodyComponent>();
    vehicleRigidbody->BodyType = EBodyType::Dynamic;
    vehicleRigidbody->Mass = 4000.0f;

    const auto boxCollider = m_VehicleEntity.AddComponent<BoxColliderComponent>();
    boxCollider->Offset = glm::vec3(0, -half_vehicle_height, 0);
    boxCollider->Size = glm::vec3(half_vehicle_width, half_vehicle_height, half_vehicle_length);

    Scene::Instance->BuildMeshEntityHierarchy(m_VehicleEntity, tankBodyModel);

    JPH::GroupFilter* filter = new JPH::GroupFilterTable;
    m_VehicleController = CreateRef<PhysicsVehicleController>(Scene->CreateBody(m_VehicleEntity));

    glm::vec3 wheel_pos[] = {
        glm::vec3(0.0f, -0.0f, 2.85f),
        glm::vec3(0.0f, -0.2f, 2.1f),
        glm::vec3(0.0f, -0.2f, 1.4f),
        glm::vec3(0.0f, -0.2f, 0.7f),
        glm::vec3(0.0f, -0.2f, 0.0f),
        glm::vec3(0.0f, -0.2f, -0.7f),
        glm::vec3(0.0f, -0.2f, -1.4f),
        glm::vec3(0.0f, -0.2f, -2.1f),
        glm::vec3(0.0f, -0.0f, -2.75f),
    };
    for (int trackIndex = 0; trackIndex < 2; ++trackIndex) {
      for (int wheelIndex = 0; wheelIndex < std::size(wheel_pos); wheelIndex++) {
        auto wheel = Scene::Instance->CreateEntity("Wheel_" + std::to_string(wheelIndex));
        Scene::Instance->BuildMeshEntityHierarchy(wheel, wheelModel);

        const float trackX = trackIndex == 0 ? half_vehicle_width : -half_vehicle_width;

        const auto wheelCollider = wheel.AddComponent<WheelColliderComponent>();
        wheelCollider->Radius = 0.3f;
        wheelCollider->Width = 0.1f;
        wheelCollider->SuspensionMinLen = 0.3f;
        wheelCollider->SuspensionMaxLen = 0.5f;
        wheelCollider->LocalPosition = glm::vec3(trackX, wheel_pos[wheelIndex].y, wheel_pos[wheelIndex].z);

        bool isEndWheel = (wheelIndex == 0) || (wheelIndex == std::size(wheel_pos) - 1);
        wheelCollider->SuspensionMaxLen = isEndWheel ? wheelCollider->SuspensionMinLen : wheelCollider->SuspensionMaxLen;

        auto wheelTransform = wheel.GetComponent<TransformComponent>();
        wheelTransform->Scale = glm::vec3(0.3, 0.3f, 0.3f);

        m_VehicleController->CreateTrackWheel(wheel, trackIndex);
        m_WheelEntities.push_back(wheel);
      }
    }

    m_VehicleController->Initalize();

    const float half_turret_width = 0.5f;
    const float half_turret_length = 1.0f;
    const float half_turret_height = 0.4f;

    const float half_barrel_length = 1.5f;
    const float barrel_radius = 0.1f;
    const float barrel_rotation_offset = 0.2f;

    m_TurretEntity = Scene::Instance->CreateEntity("TankTurret");

    const glm::vec3 turretPosition = glm::vec3(0, half_vehicle_height + half_turret_height, 0);

    const auto turretTransform = m_TurretEntity.GetComponent<TransformComponent>();
    turretTransform->Translation = turretPosition;
    turretTransform->Scale = glm::vec3(half_turret_width, half_turret_height, half_turret_length);

    const auto turretRigidbody = m_TurretEntity.AddComponent<RigidBodyComponent>();
    turretRigidbody->BodyType = EBodyType::Dynamic;
    turretRigidbody->Mass = 2000.0f;

    const auto turretCollider = m_TurretEntity.AddComponent<BoxColliderComponent>();
    // turretCollider->Offset = glm::vec3(0, -half_vehicle_height, 0);
    turretCollider->Size = glm::vec3(half_turret_width, half_turret_height, half_turret_length);

    Scene::Instance->BuildMeshEntityHierarchy(m_TurretEntity, boxModel);
    m_TurretBody = Scene->CreateBody(m_TurretEntity);

    m_TurretBarrelEntity = Scene::Instance->CreateEntity("TankBarrel");

    const auto barrelTransform = m_TurretBarrelEntity.GetComponent<TransformComponent>();
    // barrelTransform->Translation = turrentTransform->Translation + glm::vec3(0, 0, half_turret_length + half_barrel_length - barrel_rotation_offset);
    barrelTransform->Translation = turretPosition + glm::vec3(0, 0, (half_turret_length + half_barrel_length - barrel_rotation_offset));
    // barrelTransform->Translation.y = 2.29 / 2;
    barrelTransform->Scale = glm::vec3(barrel_radius, half_barrel_length, barrel_radius);
    glm::quat rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    barrelTransform->SetRotation(rotation);

    const auto barrelRigidbody = m_TurretBarrelEntity.AddComponent<RigidBodyComponent>();
    barrelRigidbody->BodyType = EBodyType::Dynamic;
    barrelRigidbody->Mass = 200.0f;

    const auto barrelCollider = m_TurretBarrelEntity.AddComponent<CylinderCollider>();
    // turretCollider->Offset = glm::vec3(0, -half_vehicle_height, 0);
    barrelCollider->Size = glm::vec3(half_barrel_length, barrel_radius, 0.0);

    Scene::Instance->BuildMeshEntityHierarchy(m_TurretBarrelEntity, boxModel);
    m_TurretBarrelBody = Scene->CreateBody(m_TurretBarrelEntity);

    JPH::BodyID bodyIds[] = {m_VehicleController->GetBody()->GetBodyId(), m_TurretBody->GetBodyId(), m_TurretBarrelBody->GetBodyId()};
    JPH::BodyLockMultiWrite multiLock(PhysicsScene::GetBodyLockInterface(), bodyIds, 3);

    JPH::Body* bodyTank = multiLock.GetBody(0);
    JPH::Body* bodyTurret = multiLock.GetBody(1);
    JPH::Body* bodyBarrel = multiLock.GetBody(2);

    auto worldTransform = Scene::Instance->GetWorldSpaceTransform(m_VehicleEntity);
    auto barrelpos = Scene::Instance->GetWorldSpaceTransform(m_TurretBarrelEntity);
    if (bodyTank && bodyTurret && bodyBarrel) {
      JPH::HingeConstraintSettings turret_hinge;
      turret_hinge.mPoint1 = turret_hinge.mPoint2 = PhysicsUtils::ToJoltVector(worldTransform.Translation) + JPH::Vec3(0, half_vehicle_height, 0);
      turret_hinge.mHingeAxis1 = turret_hinge.mHingeAxis2 = JPH::Vec3::sAxisY();
      turret_hinge.mNormalAxis1 = turret_hinge.mNormalAxis2 = JPH::Vec3::sAxisZ();
      turret_hinge.mMotorSettings = JPH::MotorSettings(0.5f, 1.0f);

      auto mTurretHinge = static_cast<JPH::HingeConstraint*>(turret_hinge.Create(*bodyTank, *bodyTurret));
      mTurretHinge->SetMotorState(JPH::EMotorState::Position);
      PhysicsScene::m_Instance->m_PhysicsSystem->AddConstraint(mTurretHinge);

      JPH::HingeConstraintSettings barrel_hinge;
      barrel_hinge.mPoint1 = barrel_hinge.mPoint2 = PhysicsUtils::ToJoltVector(barrelpos.Translation) - JPH::Vec3(0, 0, half_barrel_length);
      barrel_hinge.mHingeAxis1 = barrel_hinge.mHingeAxis2 = -JPH::Vec3::sAxisX();
      barrel_hinge.mNormalAxis1 = barrel_hinge.mNormalAxis2 = JPH::Vec3::sAxisZ();
      barrel_hinge.mLimitsMin = JPH::DegreesToRadians(-10.0f);
      barrel_hinge.mLimitsMax = JPH::DegreesToRadians(40.0f);
      barrel_hinge.mMotorSettings = JPH::MotorSettings(10.0f, 1.0f);
      auto mBarrelHinge = static_cast<JPH::HingeConstraint*>(barrel_hinge.Create(*bodyTurret, *bodyBarrel));
      mBarrelHinge->SetMotorState(JPH::EMotorState::Position);
      PhysicsScene::m_Instance->m_PhysicsSystem->AddConstraint(mBarrelHinge);
    }
  }

  void RainTrackedVehicle::OnStart() {
  }

  void RainTrackedVehicle::Update(PhysicsScene* Scene) {
    const float TURN_SPEED = 0.6f;
    const float MIN_PIVOT_VELOCITY = 1.0f;
    const float VELOCITY_THRESHOLD = 0.1f;

    float velocity = (glm::conjugate(m_VehicleController->GetBody()->GetRotation()) *
                      m_VehicleController->GetBody()->GetLinearVelocity())
                         .z;

    float mForward = 0.0f;
    float mBrake = 0.0f;
    float mLeftRatio = 1.0f;
    float mRightRatio = 1.0f;

    if (Keyboard::IsKeyPressing(Key::Space)) {
      mBrake = 1.0f;
    } else if (Keyboard::IsKeyPressing(Key::Up)) {
      mForward = 1.0f;
    } else if (Keyboard::IsKeyPressing(Key::Down)) {
      mForward = -1.0f;
    }

    bool isStationary = (mBrake == 0.0f && mForward == 0.0f && abs(velocity) < MIN_PIVOT_VELOCITY);
    bool leftTurn = Keyboard::IsKeyPressing(Key::Left);
    bool rightTurn = Keyboard::IsKeyPressing(Key::Right);

    if (leftTurn || rightTurn) {
      if (isStationary) {
        mLeftRatio = leftTurn ? -1.0f : 1.0f;
        mRightRatio = rightTurn ? -1.0f : 1.0f;
        mForward = 1.0f;
      } else {
        mLeftRatio = leftTurn ? TURN_SPEED : 1.0f;
        mRightRatio = rightTurn ? TURN_SPEED : 1.0f;
      }
    }

    bool changingDirection = (mPreviousForward * mForward < 0.0f);
    bool stillMovingWrongWay = (mForward > 0.0f && velocity < -VELOCITY_THRESHOLD) ||
                               (mForward < 0.0f && velocity > VELOCITY_THRESHOLD);

    if (changingDirection && stillMovingWrongWay) {
      mForward = 0.0f;
      mBrake = 1.0f;
    } else if (mForward != 0.0f) {
      mPreviousForward = mForward;
    }

    for (uint wheelIndex = 0; wheelIndex < m_VehicleController->GetWheelCount(); ++wheelIndex) {
      Entity& wheelEntity = m_WheelEntities[wheelIndex];
      auto transform = wheelEntity.GetComponent<TransformComponent>();

      glm::vec3 worldPos = m_VehicleController->GetWheelPosition(wheelIndex);
      glm::quat worldRot = m_VehicleController->GetWheelRotation(wheelIndex);

      transform->Translation = worldPos;
      transform->Rotation = worldRot;
    }

    // Apply inputs
    m_VehicleController->SetDriverInput(mForward, mLeftRatio, mRightRatio, mBrake);
    m_VehicleController->DrawDebug();
  }
}  // namespace Rain
