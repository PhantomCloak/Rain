#include "PhysicsVehicle.h"
#include "physics/PhysicsScene.h"
#include "physics/PhysicUtils.h"

namespace Rain {

  PhysicsVehicleController::PhysicsVehicleController(Ref<PhysicsBody> vehicleBody) {
    m_Body = vehicleBody;

    m_VehicleSettings.mDrawConstraintSize = 0.1f;
    m_VehicleSettings.mMaxPitchRollAngle = JPH::DegreesToRadians(60.0f);

    m_VehicleController = new JPH::TrackedVehicleControllerSettings;
    // Configure engine for tracked vehicle
    m_VehicleController->mEngine.mMaxTorque = 8000.0f;  // High torque for heavy tank
    m_VehicleController->mEngine.mMinRPM = 800.0f;      // Idle RPM
    m_VehicleController->mEngine.mMaxRPM = 3600.0f;     // Max RPM
    m_VehicleController->mEngine.mInertia = 50.0f;      // Engine inertia

    // Configure transmission
    m_VehicleController->mTransmission.mMode = JPH::ETransmissionMode::Auto;
    m_VehicleController->mTransmission.mGearRatios = {4.0f, 2.5f, 1.8f, 1.2f, 1.1f};  // Reverse, neutral, forward gears
    m_VehicleController->mTransmission.mSwitchTime = 0.5f;

    m_VehicleSettings.mController = m_VehicleController;

    auto* leftTrack = &m_VehicleController->mTracks[0];
    auto* rightTrack = &m_VehicleController->mTracks[1];

    // leftTrack->mDrivenWheel = 9;
    // rightTrack->mDrivenWheel = 18;
    leftTrack->mDrivenWheel = 9;    // Left track wheels: 0-9
    rightTrack->mDrivenWheel = 19;  // Right track wheels: 10-19

    // Increased track inertia and damping
    leftTrack->mInertia = 25.0f;  // Much higher inertia
    rightTrack->mInertia = 25.0f;

    leftTrack->mAngularDamping = 0.6f;  // Higher damping
    rightTrack->mAngularDamping = 0.6f;

    leftTrack->mMaxBrakeTorque = 20000.0f;  // Higher brake torque
    rightTrack->mMaxBrakeTorque = 20000.0f;
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
    JPH::VehicleTrackSettings& track = m_VehicleController->mTracks[trackIndex];
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
      // JPH::DebugRenderer::sInstance->DrawCylinder(wheel_transform, 0.5f * wheel->Width, wheel->Radius, JPH::Color::sGreen);
    }
  }
}  // namespace Rain
