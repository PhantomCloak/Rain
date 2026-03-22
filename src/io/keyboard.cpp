#include "keyboard.h"
#include <GLFW/glfw3.h>
#include <unordered_map>

namespace WebEngine {
  void* NativeWndPtr;
  static std::vector<KeyEvent> s_KeyEvents;

  void Keyboard::Setup(void* nativeWndPtr) {
    NativeWndPtr = nativeWndPtr;
  }

  bool Keyboard::IsKeyPressed(int keyCode) {
    for (const auto& event : s_KeyEvents) {
      if (event.keyCode == keyCode && event.isPressed) {
        return true;
      }
    }
    return false;
  }

  void Keyboard::Poll() {
    glfwPollEvents();
  }

  bool Keyboard::IsKeyPressing(int keyCode) {
    if (NativeWndPtr == nullptr) {
      return false;
    }
    return glfwGetKey((GLFWwindow*)NativeWndPtr, keyCode) == GLFW_PRESS;
  }

  void Keyboard::PushKeyEvent(int keyCode, bool pressed) {
    s_KeyEvents.push_back({keyCode, pressed});
  }

  std::vector<KeyEvent> Keyboard::GetPressedKeys() {
    return s_KeyEvents;
  }

  void Keyboard::FlushPressedKeys() {
    s_KeyEvents.clear();
  }
}  // namespace WebEngine
