#pragma once
#ifndef NDEBUG
#define NDEBUG
#endif

#include <webgpu/webgpu.h>
#include <glm/glm.hpp>
#include "TinyTimer.h"
#include "scene/SceneRenderer.h"

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
    m_Instance = this;
  }

  bool isRunning();

  void OnUpdate() override;
  void OnStart() override;
  void fetchTimestamps();
  TinyTimer::PerformanceCounter m_perf;
  static Application* Get();

  glm::vec2 GetWindowSize();

 private:
  Ref<SceneRenderer> m_Renderer;
  Scene* m_Scene;
  static Application* m_Instance;

 private:
  void updateDragInertia();

  void drawImgui(WGPURenderPassEncoder renderPass);
};
