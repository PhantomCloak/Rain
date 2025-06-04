#pragma once

#include "PhysicsScene.h"

#include <Jolt/Jolt.h>
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
#include <glm/glm.hpp>

namespace Rain {
  class Physics {
   public:
    static Physics* Instance;
    void Init();
    void Shutdown(){};
    Ref<PhysicsScene> CreateScene(glm::vec3 gravity) { return CreateRef<PhysicsScene>(gravity); }

   private:
  };
}  // namespace Rain
