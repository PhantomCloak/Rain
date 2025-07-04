#include "Camera.h"

namespace Rain {
  Camera::Camera(const glm::mat4& projection, const glm::mat4& unReversedProjection)
      : m_ProjectionMatrix(projection) {
  }

  Camera::Camera(const float degFov, const float width, const float height, const float nearP, const float farP)
      : m_ProjectionMatrix(glm::perspectiveFov(glm::radians(degFov), width, height, farP, nearP)) {
  }
}  // namespace Rain
