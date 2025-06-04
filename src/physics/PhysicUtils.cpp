#include "PhysicUtils.h"

namespace Rain {
  JPH::Vec3 PhysicsUtils::ToJoltVector(const glm::vec3& vector) {
    return JPH::Vec3(vector.x, vector.y, vector.z);
  }
  JPH::Quat PhysicsUtils::ToJoltQuat(const glm::quat& quat) {
    return JPH::Quat(quat.x, quat.y, quat.z, quat.w);
  }

  glm::vec3 PhysicsUtils::FromJoltVector(const JPH::Vec3& vector) {
    return *(glm::vec3*)&vector;
  }
  glm::quat PhysicsUtils::FromJoltQuat(const JPH::Quat& quat) {
    return *(glm::quat*)&quat;
  }
}  // namespace Rain
