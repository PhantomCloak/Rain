#pragma once
#include "core/Ref.h"
#include "engine/Event.h"
#include "render/SwapChain.h"
#ifndef NDEBUG
#define NDEBUG
#endif

#include <webgpu/webgpu.h>
#include <glm/glm.hpp>
#include "engine/LayerStack.h"

struct GLFWwindow;

namespace WebEngine
{
  class ImGuiLayer;
}
#if __APPLE__
#include "platform/osx/OSXWindow.h"
typedef WebEngine::OSXWindow AppWindow;
#elif __linux__
#include "platform/linux/LinuxWindow.h"
typedef WebEngine::LinuxWindow AppWindow;
#elif __EMSCRIPTEN__
#include "platform/web/WebWindow.h"
typedef WebEngine::WebWindow AppWindow;
#elif defined(_WIN32)
#include "platform/windows/WindowsWindow.h"
typedef WebEngine::WindowsWindow AppWindow;
#endif

namespace WebEngine
{
  class Application : public AppWindow
  {
   protected:
    void OnKeyPressed(KeyCode key, KeyAction action) override;
    void OnResize(int height, int width) override;

   public:
    Application(const WebEngine::WindowProps& props)
        : AppWindow(props)
    {
      m_Instance = this;
    }

    virtual void Run() override;
    virtual void OnStart() override;

    void ApplicationUpdate();
    virtual void OnEvent(Event& event) override;
    static Application* Get();

    glm::vec2 GetWindowSize();
    float GetDeltaTime() const { return m_DeltaTime; }
    Ref<WebEngine::SwapChain> GetSwapChain() { return m_SwapChain; }

   private:
    Ref<WebEngine::SwapChain> m_SwapChain;
    LayerStack m_Layers;
    ImGuiLayer* m_ImGuiLayer = nullptr;
    static Application* m_Instance;
    float m_DeltaTime = 0.0f;
    float m_LastFrameTime = 0.0f;
  };
}  // namespace WebEngine
