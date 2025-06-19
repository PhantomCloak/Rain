#include "PhysicsWheel.h"

#include <Physics/Vehicle/VehicleConstraint.h>

#include "scene/Components.h"
#include "PhysicUtils.h"

namespace Rain {
  PhysicsWheel::PhysicsWheel(JPH::VehicleConstraintSettings& vehicleSettings, Entity entity) {
    const WheelColliderComponent& wheelColliderComponent = entity.GetComponent<WheelColliderComponent>();
    JPH::WheelSettingsTV* wheelSetting = new JPH::WheelSettingsTV;
    wheelSetting->mPosition = PhysicsUtils::ToJoltVector(wheelColliderComponent.LocalPosition);
    wheelSetting->mRadius = wheelColliderComponent.Radius;
    wheelSetting->mWidth = wheelColliderComponent.Width;
    wheelSetting->mSuspensionMinLength = wheelColliderComponent.SuspensionMinLen;

    // Improved suspension settings
    wheelSetting->mSuspensionSpring.mFrequency = 2.5f;  // Stiffer suspension
    wheelSetting->mSuspensionSpring.mDamping = 0.8f;    // Higher damping for stability

    // CRITICAL: Much higher lateral friction for tracked vehicles
    wheelSetting->mLongitudinalFriction = 1.8f;  // Forward/backward grip
    wheelSetting->mLateralFriction = 4.5f;       // Very high lateral friction - prevents sliding

    vehicleSettings.mWheels.push_back(wheelSetting);
    Width = wheelColliderComponent.Width;
    Radius = wheelColliderComponent.Radius;
  }
}  // namespace Rain
