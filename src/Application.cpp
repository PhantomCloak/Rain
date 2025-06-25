// #include "imgui.h"
#include "scene/Entity.h" #include "scene/Scene.h"
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
#include "render/GPUAllocator.h"
#include "render/Render.h"
#include "render/ResourceManager.h"

#if __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <Tracy.hpp>
#endif

namespace Rain {
  std::unique_ptr<Render> render;
  Application* Application::m_Instance;

  // This place still heavily under WIP
  void Application::OnStart() {
    Rain::Log::Init();

    RN_LOG("=== Rain Engine Starting ===");
    RN_LOG("OS: {}", SysInfo::OSName());
    RN_LOG("CPU: {}", SysInfo::CPUName());
    RN_LOG("Total Cores: {}", SysInfo::CoreCount());
    RN_LOG("Total Memory (RAM): {}", SysInfo::TotalMemory());

    render = std::make_unique<Render>();
    render->Init(GetNativeWindow());

    GPUAllocator::Init();

    Rain::ResourceManager::LoadTexture("T_Default", RESOURCE_DIR "/textures/placeholder.jpeg");

    m_Renderer = CreateRef<SceneRenderer>();
    m_Renderer->Init();

    m_Scene = new Scene("Test Scene");
    m_Scene->Init();

    Cursor::Setup(render->m_Window);
    Keyboard::Setup(render->m_Window);
    Cursor::CaptureMouse(true);
  }

  void Application::OnResize(int height, int width) {
    // render->m_swapChainDesc.height = height;
    // render->m_swapChainDesc.width = width;
    // render->m_swapChain = render->BuildSwapChain(render->m_swapChainDesc, render->m_device, render->m_surface);

    m_Renderer->SetViewportSize(height, width);
  }

  void Application::OnUpdate() {
#ifndef __EMSCRIPTEN__
    FrameMark;
#endif
    glfwPollEvents();
    m_Scene->OnUpdate();
    m_Scene->OnRender(m_Renderer);
  }

  void Application::OnMouseClick(Rain::MouseCode button) {
    // ImGuiIO& io = ImGui::GetIO();
    // if (io.WantCaptureMouse) {
    //   return;
    // }
    //  Cursor::CaptureMouse(true);
  }

  void Application::OnMouseMove(double xPos, double yPos) {
    m_Scene->OnMouseMove(xPos, yPos);
  }

  void Application::OnKeyPressed(KeyCode key, KeyAction action) {
    if (key == Rain::Key::Escape && action == Rain::Key::RN_KEY_RELEASE) {
      Cursor::CaptureMouse(false);
    }
  }

  bool Application::isRunning() {
    return !glfwWindowShouldClose(render->m_Window);
  }

  Application* Application::Get() {
    return m_Instance;
  }

  glm::vec2 Application::GetWindowSize() {
    int screenWidth, screenHeight;
    glfwGetFramebufferSize((GLFWwindow*)GetNativeWindow(), &screenWidth, &screenHeight);
    return glm::vec2(screenWidth, screenHeight);
  }
}  // namespace Rain
