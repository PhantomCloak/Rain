#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Vehicle/TrackedVehicleController.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <glm/glm.hpp>

#include "scene/Entity.h"

namespace WebEngine {
  class PhysicsWheel {
   public:
    PhysicsWheel(JPH::VehicleConstraintSettings& vehicleSettings, Entity entity);

    glm::vec3 GetRotationEuler();
    glm::vec3 GetPosition();

    glm::mat4 GetWheelWorldTransform();

    float Radius;
    float Width;
  };
}  // namespace WebEngine
