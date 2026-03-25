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
#include <memory>

namespace WebEngine
{
  class Physics
  {
   public:
    void Init();
    void Shutdown() {};
    static std::unique_ptr<PhysicsScene> CreateScene(glm::vec3 gravity) { return std::make_unique<PhysicsScene>(gravity); };

   private:
  };
}  // namespace WebEngine
