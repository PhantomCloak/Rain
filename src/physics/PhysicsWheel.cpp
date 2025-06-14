#include "PhysicsWheel.h"

#include <Physics/Vehicle/VehicleConstraint.h>

#include "scene/Components.h"
#include "PhysicUtils.h"

namespace Rain {
  PhysicsWheel::PhysicsWheel(JPH::VehicleConstraintSettings& vehicleSettings, Entity entity) {
    const auto& wheelColliderComponent = entity.GetComponent<WheelColliderComponent>();

    JPH::WheelSettingsTV* wheelSetting = new JPH::WheelSettingsTV;
    wheelSetting->mPosition = PhysicsUtils::ToJoltVector(wheelColliderComponent->LocalPosition);
    // wheelSetting->mPosition.SetX(trackX);
    wheelSetting->mRadius = wheelColliderComponent->Radius;
    wheelSetting->mWidth = wheelColliderComponent->Width;
    wheelSetting->mSuspensionMinLength = wheelColliderComponent->SuspensionMinLen;
    wheelSetting->mSuspensionSpring.mFrequency = 1.0f;

    vehicleSettings.mWheels.push_back(wheelSetting);

    Width = wheelColliderComponent->Width;
    Radius = wheelColliderComponent->Radius;
  }
}  // namespace Rain
