#include "PhysicsVehicle.h"
#include "physics/PhysicsScene.h"
#include "physics/PhysicUtils.h"

namespace Rain {

  PhysicsVehicleController::PhysicsVehicleController(Ref<PhysicsBody> vehicleBody) {
    m_Body = vehicleBody;

    m_VehicleSettings.mDrawConstraintSize = 0.1f;
    m_VehicleSettings.mMaxPitchRollAngle = JPH::DegreesToRadians(60.0f);

    m_VehicleContrtoller = new JPH::TrackedVehicleControllerSettings;
    m_VehicleSettings.mController = m_VehicleContrtoller;

    auto* leftTrack = &m_VehicleContrtoller->mTracks[0];
    auto* rightTrack = &m_VehicleContrtoller->mTracks[1];

    leftTrack->mDrivenWheel = 8;
    rightTrack->mDrivenWheel = 17;

    m_TrackSettings.push_back(leftTrack);
    m_TrackSettings.push_back(rightTrack);
  }

  glm::vec3 PhysicsVehicleController::GetWheelPosition(uint wheelIndex) const {
    if (!m_VehicleConstraint || wheelIndex >= m_VehicleConstraint->GetWheels().size()) {
      return glm::vec3(0.0f);
    }

    JPH::RMat44 wheelTransform = m_VehicleConstraint->GetWheelWorldTransform(
        wheelIndex,
        JPH::Vec3::sAxisY(),
        JPH::Vec3::sAxisX());

    JPH::Vec3 position = wheelTransform.GetTranslation();
    return PhysicsUtils::FromJoltVector(position);
  }

  glm::quat PhysicsVehicleController::GetWheelRotation(uint wheelIndex) const {
    if (!m_VehicleConstraint || wheelIndex >= m_VehicleConstraint->GetWheels().size()) {
      return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    JPH::RMat44 wheelTransform = m_VehicleConstraint->GetWheelWorldTransform(
        wheelIndex,
        JPH::Vec3::sAxisY(),
        JPH::Vec3::sAxisX());

    JPH::Quat rotation = wheelTransform.GetQuaternion();
    return PhysicsUtils::FromJoltQuat(rotation);
  }

  glm::mat4 PhysicsVehicleController::GetWheelTransform(uint wheelIndex) const {
    if (!m_VehicleConstraint || wheelIndex >= m_VehicleConstraint->GetWheels().size()) {
      return glm::mat4(1.0f);
    }

    JPH::RMat44 wheelTransform = m_VehicleConstraint->GetWheelWorldTransform(
        wheelIndex,
        JPH::Vec3::sAxisY(),
        JPH::Vec3::sAxisX());

    // return PhysicsUtils::FromJoltQuat(wheelTransform);
  }

  void PhysicsVehicleController::CreateTrackWheel(Entity wheelEntity, int trackIndex) {
    JPH::VehicleTrackSettings& track = m_VehicleContrtoller->mTracks[trackIndex];
    track.mWheels.push_back((uint)m_VehicleSettings.mWheels.size());

    m_Wheels.push_back(PhysicsScene::m_Instance->CreateWheel(m_VehicleSettings, wheelEntity));
  }

  void PhysicsVehicleController::Initalize() {
    JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_Body->GetBodyId());
    JPH::Body& body = bodyLock.GetBody();

    m_VehicleConstraint = new JPH::VehicleConstraint(body, m_VehicleSettings);
    m_VehicleConstraint->SetVehicleCollisionTester(new JPH::VehicleCollisionTesterRay(Layers::MOVING));

    PhysicsScene::m_Instance->m_PhysicsSystem->AddConstraint(m_VehicleConstraint);
    PhysicsScene::m_Instance->m_PhysicsSystem->AddStepListener(m_VehicleConstraint);
  }

  Ref<PhysicsBody> PhysicsVehicleController::GetBody() {
    return m_Body;
  }

  void PhysicsVehicleController::SetDriverInput(float forward, float left, float right, float brake) {
    m_Body->Activate();
    static_cast<JPH::TrackedVehicleController*>(m_VehicleConstraint->GetController())->SetDriverInput(forward, left, right, brake);
  }

  void PhysicsVehicleController::DrawDebug() {
    for (uint wheelIndex = 0; wheelIndex < m_VehicleConstraint->GetWheels().size(); ++wheelIndex) {
      const Ref<PhysicsWheel> wheel = m_Wheels[wheelIndex];

      JPH::RMat44 wheel_transform = m_VehicleConstraint->GetWheelWorldTransform(wheelIndex, JPH::Vec3::sAxisY(), JPH::Vec3::sAxisX());  // The cylinder we draw is aligned with Y so we specify that as rotational axis
      JPH::DebugRenderer::sInstance->DrawCylinder(wheel_transform, 0.5f * wheel->Width, wheel->Radius, JPH::Color::sGreen);
    }
  }
}  // namespace Rain
