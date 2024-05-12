#pragma once

#include <webgpu/webgpu.h>
#include <glm/glm.hpp>

struct GLFWwindow;

#if __APPLE__
#include "platform/osx/OSXWindow.h"
typedef Rain::OSXWindow AppWindow;
#elif __EMSCRIPTEN__
#include "platform/web/WebWindow.h"
typedef Rain::WebWindow AppWindow;
#endif

class Application : public AppWindow {
 protected:
  void OnKeyPressed(KeyCode key, KeyAction action) override;
  void OnMouseMove(double xPos, double yPos) override;
  void OnMouseClick(Rain::MouseCode button) override;
  void OnResize(int height, int width) override;

 public:
  Application(const Rain::WindowProps& props)
      : AppWindow(props) {
  }

  bool isRunning();

  void OnUpdate() override;
  void OnStart() override;
 private:
  void updateDragInertia();

  void updateGui(WGPURenderPassEncoder renderPass);
};
