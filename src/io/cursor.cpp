#include "cursor.h"
#include <GLFW/glfw3.h>
#include "core/Log.h"

#if EDITOR
#include "../../../editor/editor.h"
#endif

namespace Rain
{
  static GLFWwindow* NativeWndPtr;
  static bool isMouseCapturing = false;
  static bool currentMouseButtonStates[GLFW_MOUSE_BUTTON_LAST + 1] = {false};
  static bool prevMouseButtonStates[GLFW_MOUSE_BUTTON_LAST + 1] = {false};
  static float scrollDelta = 0.0f;

  static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
  {
    scrollDelta += static_cast<float>(yoffset);
  }

  void Cursor::Setup(void* window)
  {
    if (window == nullptr)
    {
      RN_LOG("Window pointer passed null");
    }

    NativeWndPtr = (GLFWwindow*)window;
    glfwSetScrollCallback(NativeWndPtr, ScrollCallback);
  }
  void Cursor::Update()
  {
    if (NativeWndPtr == nullptr)
    {
      return;
    }

    for (int i = 0; i <= GLFW_MOUSE_BUTTON_LAST; i++)
    {
      prevMouseButtonStates[i] = currentMouseButtonStates[i];
      currentMouseButtonStates[i] = glfwGetMouseButton(NativeWndPtr, i) == GLFW_PRESS;
    }
  }

  glm::vec2 Cursor::GetCursorPosition()
  {
    double x = 0;
    double y = 0;

    if (NativeWndPtr == nullptr)
    {
      return glm::vec2(0, 0);
    }

    glfwGetCursorPos(NativeWndPtr, &x, &y);
    return glm::vec2(x, y);
  }

  bool Cursor::HasLeftCursorClicked()
  {
    if (NativeWndPtr == nullptr)
    {
      return false;
    }

    return glfwGetMouseButton(NativeWndPtr, 0);
  }

  bool Cursor::HasRightCursorClicked()
  {
    if (NativeWndPtr == nullptr)
    {
      return false;
    }

    return glfwGetMouseButton(NativeWndPtr, 1);
  }

  bool Cursor::WasLeftCursorPressed()
  {
    return currentMouseButtonStates[0] && !prevMouseButtonStates[0];
  }

  bool Cursor::WasRightCursorPressed()
  {
    return currentMouseButtonStates[1] && !prevMouseButtonStates[1];
  }

  void Cursor::CaptureMouse(bool shouldCapture)
  {
    if (NativeWndPtr == nullptr)
    {
      return;
    }

    if (shouldCapture)
    {
      glfwSetInputMode((GLFWwindow*)NativeWndPtr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    else
    {
      glfwSetInputMode((GLFWwindow*)NativeWndPtr, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    isMouseCapturing = shouldCapture;
  }

  bool Cursor::IsMouseCaptured()
  {
    return isMouseCapturing;
  }

  bool Cursor::IsMiddleMouseHeld()
  {
    return currentMouseButtonStates[GLFW_MOUSE_BUTTON_MIDDLE];
  }

  float Cursor::GetScrollDelta()
  {
    return scrollDelta;
  }

  void Cursor::ResetScrollDelta()
  {
    scrollDelta = 0.0f;
  }
}  // namespace Rain
