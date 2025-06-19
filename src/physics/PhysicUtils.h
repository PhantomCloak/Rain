#pragma once
#ifndef NDEBUG
#define NDEBUG
#endif
#include <Jolt.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Rain {
  class PhysicsUtils {
   public:
    static JPH::Vec3 ToJoltVector(const glm::vec3& vector);
    static JPH::Quat ToJoltQuat(const glm::quat& quat);
    static glm::vec3 FromJoltVector(const JPH::Vec3& vector);
    static glm::quat FromJoltQuat(const JPH::Quat& quat);
  };
}  // namespace Rain
