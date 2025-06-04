#include "cursor.h"
#include <GLFW/glfw3.h>

#if EDITOR
#include "../../../editor/editor.h"
#endif

namespace Rain {
  static GLFWwindow* NativeWndPtr;
  static bool isMouseCapturing = false;
  static bool currentMouseButtonStates[GLFW_MOUSE_BUTTON_LAST + 1] = {false};
  static bool prevMouseButtonStates[GLFW_MOUSE_BUTTON_LAST + 1] = {false};

  void Cursor::Setup(void* window) {
    NativeWndPtr = (GLFWwindow*)window;
  }
  void Cursor::Update() {
    // Store previous frame's button states
    for (int i = 0; i <= GLFW_MOUSE_BUTTON_LAST; i++) {
      prevMouseButtonStates[i] = currentMouseButtonStates[i];
      currentMouseButtonStates[i] = glfwGetMouseButton(NativeWndPtr, i) == GLFW_PRESS;
    }
  }

  glm::vec2 Cursor::GetCursorPosition() {
    double x = 0;
    double y = 0;

    glfwGetCursorPos(NativeWndPtr, &x, &y);
    return glm::vec2(x, y);
  }

  bool Cursor::HasLeftCursorClicked() {
    return glfwGetMouseButton(NativeWndPtr, 0);
  }

  bool Cursor::HasRightCursorClicked() {
    return glfwGetMouseButton(NativeWndPtr, 1);
  }

  bool Cursor::WasLeftCursorPressed() {
    return currentMouseButtonStates[0] && !prevMouseButtonStates[0];
  }

  bool Cursor::WasRightCursorPressed() {
    return currentMouseButtonStates[1] && !prevMouseButtonStates[1];
  }

  void Cursor::CaptureMouse(bool shouldCapture) {
    if (shouldCapture) {
      glfwSetInputMode((GLFWwindow*)NativeWndPtr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
      glfwSetInputMode((GLFWwindow*)NativeWndPtr, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    isMouseCapturing = shouldCapture;
  }

  bool Cursor::IsMouseCaptured() {
    return isMouseCapturing;
  }
}  // namespace Rain
