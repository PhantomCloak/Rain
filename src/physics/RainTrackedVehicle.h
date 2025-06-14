#pragma once
#include "engine/Actor.h"
#include "glm/glm.hpp"
#include "physics/PhysicsScene.h"
#include "physics/PhysicsVehicle.h"
#include <vector>

namespace Rain {
  struct TrackedWheelProps {
    glm::vec3 CenterOfMassOffset = glm::vec3(0.0f);
    float VehicleWidth;
    float VehicleHeight;
    float VehicleLenght;
    float MaxPitchRollAngle = 60.0f;
    float Mass = 4000.0f;
  };

  class RainTrackedVehicle {
   public:
    RainTrackedVehicle() {};
    void OnStart();
    void CreateVehicle(PhysicsScene* Scene);
    void Update(PhysicsScene* Scene);

   private:
    TrackedWheelProps m_Props;
    Ref<PhysicsBody> m_TurretBody;
    Ref<PhysicsBody> m_TurretBarrelBody;
    Ref<PhysicsVehicleController> m_VehicleController;

    Entity m_VehicleEntity;
    Entity m_TurretEntity;
    Entity m_TurretBarrelEntity;
    std::vector<Entity> m_WheelEntities;

    float mPreviousForward = 1.0f;  ///< Keeps track of last car direction so we know when to brake and when to accelerate
    float mTurretHeading = 0.0f;
    float mBarrelPitch = 0.0f;
  };
}  // namespace Rain
