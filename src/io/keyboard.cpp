#include "keyboard.h"
#include <GLFW/glfw3.h>
#include <unordered_map>

namespace Rain {
  void* NativeWndPtr;

  void Keyboard::Setup(void* nativeWndPtr) {
    NativeWndPtr = nativeWndPtr;
  }

  bool Keyboard::IsKeyPressed(int keyCode) {
        if (NativeWndPtr == nullptr) {
      return false;
    }
    return glfwGetKey((GLFWwindow*)NativeWndPtr, keyCode);
  }

  void Keyboard::Poll() {
    glfwPollEvents();
  }

  bool Keyboard::IsKeyPressing(int keyCode) {
    if (NativeWndPtr == nullptr) {
      return false;
    }
    return glfwGetKey((GLFWwindow*)NativeWndPtr, keyCode);
  }

  void Keyboard::FlushPressedKeys() {
  }
}  // namespace Rain
