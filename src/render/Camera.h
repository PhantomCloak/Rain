#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Rain {
  class Camera {
   public:
    Camera() = default;
    Camera(const glm::mat4& projection, const glm::mat4& unReversedProjection);
    Camera(const float degFov, const float width, const float height, const float nearP, const float farP);
    virtual ~Camera() = default;

    const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
    const glm::mat4& GetUnReversedProjectionMatrix() const { return m_UnReversedProjectionMatrix; }

    void SetPerspectiveProjectionMatrix(const float radFov, const float width, const float height, const float nearP, const float farP) {
      m_ProjectionMatrix = glm::perspectiveFov(radFov, width, height, nearP, farP);
      m_UnReversedProjectionMatrix = glm::perspectiveFov(radFov, width, height, farP, nearP);
    }

   private:
    glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
    glm::mat4 m_UnReversedProjectionMatrix = glm::mat4(1.0f);
  };
}  // namespace Rain
