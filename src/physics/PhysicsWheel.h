#pragma once

#include <Jolt.h>
#include <Physics/Vehicle/TrackedVehicleController.h>
#include <Physics/Body/BodyInterface.h>
#include <glm/glm.hpp>

#include "scene/Entity.h"

namespace Rain {
  class PhysicsWheel {
   public:
    PhysicsWheel(JPH::VehicleConstraintSettings& vehicleSettings, Entity entity);

    glm::vec3 GetRotationEuler();
    glm::vec3 GetPosition();

    glm::mat4 GetWheelWorldTransform();

    float Radius;
    float Width;
  };
}  // namespace Rain
