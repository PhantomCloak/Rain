#pragma once

#include <glm/glm.hpp>

namespace Rain {
  class Camera;
  class Cursor {
   public:
    static void Setup(void* window);
    static glm::vec2 GetCursorPosition();
    static glm::vec2 GetCursorWorldPosition(glm::vec2 screenPoint, Camera cam);
    static bool HasLeftCursorClicked();
    static bool HasRightCursorClicked();
    static bool WasLeftCursorPressed();
    static void Update();
    static bool WasRightCursorPressed();
    static void CaptureMouse(bool shouldCapture);
    static bool IsMouseCaptured();
  };
}  // namespace Rain
