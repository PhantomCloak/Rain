// #include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include "core/Ref.h"
#include "render/RenderWGPU.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#define RN_DEBUG
#include <GLFW/glfw3.h>
#include <unistd.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include "Application.h"

#include "core/Log.h"
#include "core/SysInfo.h"
#include "core/Thread.h"
#include "io/cursor.h"
#include "io/keyboard.h"
#include "EditorLayer.h"
#include "engine/ImGuiLayer.h"
#include "render/Render.h"
#include "render/ResourceManager.h"

#include "imgui.h"

#if __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <Tracy.hpp>
#endif

namespace Rain
{
  std::unique_ptr<Render> m_Render;
  Application* Application::m_Instance;

  // This place still heavily under WIP
  void Application::OnStart()
  {
    Rain::Log::Init();

    RN_LOG("=== Rain Engine Starting ===");
    RN_LOG("OS: {}", SysInfo::OSName());
    RN_LOG("CPU: {}", SysInfo::CPUName());
    RN_LOG("Total Cores: {}", SysInfo::CoreCount());
    RN_LOG("Total Memory (RAM): {}", SysInfo::TotalMemory());

    m_Render = std::make_unique<RenderWGPU>();

    const auto InitializeScene = [this]()
    {
      RN_LOG("Render API is ready!");
      Rain::ResourceManager::LoadTexture("T_Default", RESOURCE_DIR "/textures/placeholder.jpeg");

      if (m_Render)
      {
        Cursor::Setup(m_Render->GetActiveWindow());
        Keyboard::Setup(m_Render->GetActiveWindow());
      }

      // Cursor::CaptureMouse(true);  // TODO: Shouldn't be here

      m_ImGuiLayer = new ImGuiLayer();
      m_ImGuiLayer->OnAttach();
    };

    if (m_Render)
    {
      m_SwapChain = CreateRef<Rain::SwapChain>();
      m_Render->OnReady = InitializeScene;
      m_Render->Init(GetNativeWindow());

      while (!m_Render->IsReady())
      {
        m_Render->Tick();
        Thread::Sleep(16);
      }
    }
    else
    {
      InitializeScene();
    }

    m_Layers.PushLayer(new EditorLayer());
  }

  void Application::OnResize(int height, int width)
  {
    // m_Renderer->SetViewportSize(height, width);
  }

  void Application::Run()
  {
    for (Layer* layer : m_Layers)
    {
      layer->OnAttach();
    }

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(
        [](void* userData)
        {
          Application& thisClass = *reinterpret_cast<Application*>(userData);
          thisClass.ApplicationUpdate();
        },
        (void*)this,
        0, true);
#else
    while (!glfwWindowShouldClose(m_Window))
    {
      ApplicationUpdate();
    }

    for (Layer* layer : m_Layers)
    {
      layer->OnDeattach();
    }
#endif
  }

  void Application::ApplicationUpdate()
  {
#ifndef __EMSCRIPTEN__
    FrameMark;
#endif

    glfwPollEvents();

    float currentTime = static_cast<float>(glfwGetTime());
    m_DeltaTime = currentTime - m_LastFrameTime;
    m_LastFrameTime = currentTime;

    m_SwapChain->BeginFrame();

    m_ImGuiLayer->Begin();
    for (Layer* layer : m_Layers)
    {
      layer->OnUpdate(m_DeltaTime);
    }

    for (Layer* layer : m_Layers)
    {
      layer->OnRenderImGui();
    }
    m_ImGuiLayer->End();

    m_SwapChain->Present();

    if (m_Render != nullptr)
    {
      m_Render->Tick();
    }
  }

  void Application::OnMouseClick(Rain::MouseCode button)
  {
    // ImGuiIO& io = ImGui::GetIO();
    // if (io.WantCaptureMouse) {
    //   return;
    // }
    //  Cursor::CaptureMouse(true);
  }

  void Application::OnMouseMove(double xPos, double yPos)
  {
    // if (m_Scene)
    //{
    //   m_Scene->OnMouseMove(xPos, yPos);
    // }
  }

  void Application::OnKeyPressed(KeyCode key, KeyAction action)
  {
    if (key == Rain::Key::Escape && action == Rain::Key::RN_KEY_RELEASE)
    {
      Cursor::CaptureMouse(false);
    }
  }

  Application* Application::Get()
  {
    return m_Instance;
  }

  glm::vec2 Application::GetWindowSize()
  {
    int screenWidth, screenHeight;
    glfwGetFramebufferSize((GLFWwindow*)GetNativeWindow(), &screenWidth, &screenHeight);
    return glm::vec2(screenWidth, screenHeight);
  }
}  // namespace Rain
