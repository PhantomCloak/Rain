#pragma once
#include "core/Ref.h"
#include "render/SwapChain.h"
#ifndef NDEBUG
#define NDEBUG
#endif

#include <webgpu/webgpu.h>
#include <glm/glm.hpp>
#include "engine/LayerStack.h"

struct GLFWwindow;

namespace Rain
{
  class ImGuiLayer;
}
#if __APPLE__
#include "platform/osx/OSXWindow.h"
typedef Rain::OSXWindow AppWindow;
#elif __linux__
#include "platform/linux/LinuxWindow.h"
typedef Rain::LinuxWindow AppWindow;
#elif __EMSCRIPTEN__
#include "platform/web/WebWindow.h"
typedef Rain::WebWindow AppWindow;
#endif

namespace Rain
{
  class Application : public AppWindow
  {
   protected:
    void OnKeyPressed(KeyCode key, KeyAction action) override;
    void OnMouseMove(double xPos, double yPos) override;
    void OnMouseClick(Rain::MouseCode button) override;
    void OnResize(int height, int width) override;

   public:
    Application(const Rain::WindowProps& props)
        : AppWindow(props)
    {
      m_Instance = this;
    }

    virtual void Run() override;
    virtual void OnStart() override;
    void ApplicationUpdate();
    void fetchTimestamps();
    static Application* Get();

    glm::vec2 GetWindowSize();
    float GetDeltaTime() const { return m_DeltaTime; }
    Ref<Rain::SwapChain> GetSwapChain() { return m_SwapChain; }

   private:
    Ref<Rain::SwapChain> m_SwapChain;
    LayerStack m_Layers;
    ImGuiLayer* m_ImGuiLayer = nullptr;
    static Application* m_Instance;
    float m_DeltaTime = 0.0f;
    float m_LastFrameTime = 0.0f;
  };
}  // namespace Rain
