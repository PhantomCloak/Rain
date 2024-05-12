#include <GLFW/glfw3.h>
#include "keyboard.h"
#include <unordered_map>

void* NativeWndPtr;

void Keyboard::Setup(void* nativeWndPtr) {
	NativeWndPtr = nativeWndPtr;
}

bool Keyboard::IsKeyPressed(int keyCode) {
  return glfwGetKey((GLFWwindow*)NativeWndPtr, keyCode);
}

void Keyboard::Poll() {
  glfwPollEvents();
}

bool Keyboard::IsKeyPressing(int keyCode) {
  return glfwGetKey((GLFWwindow*)NativeWndPtr, keyCode);
}

void Keyboard::FlushPressedKeys() {
}
