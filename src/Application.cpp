#pragma once
#include "scene/Components.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#define RN_DEBUG
#include <GLFW/glfw3.h>
#include <unistd.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include "Application.h"

#include "Cam.h"

#include "core/Log.h"
#include "io/cursor.h"
#include "io/keyboard.h"
#include "render/GPUAllocator.h"
#include "render/Render.h"
#include "render/ResourceManager.h"

#include <tint/tint.h>

#if __EMSCRIPTEN__
#include <emscripten.h>
#include "platform/web/web_window.h"
#endif

std::unique_ptr<Render> render;
Application* Application::m_Instance;

// This place still heavily under WIP
void Application::OnStart() {
  render = std::make_unique<Render>();
  render->Init(GetNativeWindow());

  Rain::Log::Init();
  GPUAllocator::Init();

  Rain::ResourceManager::LoadTexture("T_Default", RESOURCE_DIR "/textures/placeholder.jpeg");

  m_Renderer = CreateRef<SceneRenderer>();
  m_Renderer->Init();

  m_Scene = new Scene("Test Scene");
  m_Scene->Init();

  Cursor::Setup(render->m_window);
  Keyboard::Setup(render->m_window);
  Cursor::CaptureMouse(true);
}

void Application::OnResize(int height, int width) {
  render->m_swapChainDesc.height = height;
  render->m_swapChainDesc.width = width;
  render->m_swapChain = render->BuildSwapChain(render->m_swapChainDesc, render->m_device, render->m_surface);
}

void Application::OnUpdate() {
  glfwPollEvents();

  m_Scene->OnUpdate();
  m_Scene->OnRender(m_Renderer);
}

void Application::OnMouseClick(Rain::MouseCode button) {
  Cursor::CaptureMouse(true);
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
  return !glfwWindowShouldClose(render->m_window);
}

Application* Application::Get() {
  return m_Instance;
}

glm::vec2 Application::GetWindowSize() {
  int screenWidth, screenHeight;
  glfwGetFramebufferSize((GLFWwindow*)GetNativeWindow(), &screenWidth, &screenHeight);
  return glm::vec2(screenWidth, screenHeight);
}
