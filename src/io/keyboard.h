#pragma once

#include <vector>

namespace WebEngine {
  struct KeyEvent {
    int keyCode;
    bool isPressed;
  };

  class Keyboard {
   public:
    static void Setup(void* nativeWndPtr);
    static bool IsKeyPressed(int keyCode);
    static bool IsKeyPressing(int keyCode);
    static void PushKeyEvent(int keyCode, bool pressed);
    static std::vector<KeyEvent> GetPressedKeys();
    static void Poll();
    static void FlushPressedKeys();
  };
}  // namespace WebEngine
