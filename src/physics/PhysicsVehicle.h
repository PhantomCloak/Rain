#pragma once

#include <Jolt.h>
#include <Physics/Vehicle/TrackedVehicleController.h>
#include <Physics/Body/BodyInterface.h>
#include <glm/glm.hpp>

#include "physics/PhysicsBody.h"
#include "scene/Entity.h"
#include "physics/PhysicsWheel.h"

namespace Rain {
  class PhysicsVehicleController {
   public:
    PhysicsVehicleController(Ref<PhysicsBody> vehicleBody);

    glm::vec3 GetWheelPosition(uint wheelIndex) const;
    glm::quat GetWheelRotation(uint wheelIndex) const;
    glm::mat4 GetWheelTransform(uint wheelIndex) const;

    uint32_t GetWheelCount() { return m_Wheels.size(); };
    Ref<PhysicsWheel> GetWheel(uint32_t wheelIndex) { return m_Wheels[wheelIndex]; };

    void CreateTrackWheel(Entity wheelEntity, int trackIndex);
    void Initalize();

    void SetDriverInput(float forward, float left, float right, float brake);

    Ref<PhysicsBody> GetBody();

    void DrawDebug();

   private:
    std::vector<Ref<PhysicsWheel>> m_Wheels;

    Ref<PhysicsBody> m_Body;
    JPH::VehicleConstraintSettings m_VehicleSettings;
    JPH::TrackedVehicleControllerSettings* m_VehicleContrtoller;
    JPH::VehicleConstraint* m_VehicleConstraint;
    std::vector<JPH::VehicleTrackSettings*> m_TrackSettings;
  };
}  // namespace Rain
