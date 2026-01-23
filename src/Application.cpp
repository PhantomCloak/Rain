// #include "imgui.h"
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
#include "io/cursor.h"
#include "io/keyboard.h"
#include "render/Render.h"
#include "render/ResourceManager.h"

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

      m_Renderer = CreateRef<SceneRenderer>();
      m_Renderer->Init();

      m_Scene = std::make_unique<Scene>("Test Scene");
      m_Scene->Init();

      if (m_Render)
      {
        Cursor::Setup(m_Render->GetActiveWindow());
        Keyboard::Setup(m_Render->GetActiveWindow());
      }
      Cursor::CaptureMouse(true);
    };

    if (m_Render)
    {
      m_Render->OnReady = InitializeScene;
      m_Render->Init(GetNativeWindow());
    }
    else
    {
      InitializeScene();
    }
  }

  void Application::OnResize(int height, int width)
  {
    m_Renderer->SetViewportSize(height, width);
  }

  void Application::OnUpdate()
  {
#ifndef __EMSCRIPTEN__
    FrameMark;
#endif
    glfwPollEvents();

    if (m_Scene != nullptr)
    {
      m_Scene->OnUpdate();
      if (m_Renderer != nullptr && m_Render->IsReady())
      {
        m_Scene->OnRender(m_Renderer);
      }
    }

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
    m_Scene->OnMouseMove(xPos, yPos);
  }

  void Application::OnKeyPressed(KeyCode key, KeyAction action)
  {
    if (key == Rain::Key::Escape && action == Rain::Key::RN_KEY_RELEASE)
    {
      Cursor::CaptureMouse(false);
    }
  }

  bool Application::isRunning()
  {
    if (const auto ptr = m_Render->GetActiveWindow())
    {
      return !glfwWindowShouldClose(ptr);
    }

    return true;
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
