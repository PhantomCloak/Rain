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

extern "C" WGPUSurface glfwGetWGPUSurface(WGPUInstance instance, GLFWwindow* window);

std::unique_ptr<Render> render;
Application* Application::m_Instance;
std::shared_ptr<PlayerCamera> Player;

// This place still heavily under WIP
void Application::OnStart() {
  render = std::make_unique<Render>();
  m_Scene = new Scene("Test Scene");

  WGPUInstance instance = render->CreateInstance();
  render->m_window = static_cast<GLFWwindow*>(GetNativeWindow());

#if __EMSCRIPTEN__
  render->m_surface = htmlGetCanvasSurface(instance, "canvas");
#else
  render->m_surface = glfwGetWGPUSurface(instance, render->m_window);
#endif

  render->Init(render->m_window, instance);
  Rain::Log::Init();
  GPUAllocator::Init();

  m_Renderer = CreateRef<SceneRenderer>();
  m_Renderer->Init();

  Cursor::Setup(render->m_window);
  Keyboard::Setup(render->m_window);
  Cursor::CaptureMouse(true);

  Player = std::make_shared<PlayerCamera>();
  Player->Position.y = 0;
  Player->Position.z = 0;

  Rain::ResourceManager::LoadTexture("T_Default", RESOURCE_DIR "/textures/placeholder.jpeg");

  Ref<MeshSource> exampleModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/models/SponzaExp5.gltf");
  Entity sampleEntity = m_Scene->CreateEntity("Box");
  sampleEntity.GetComponent<TransformComponent>()->Translation = glm::vec3(0, 10, 0);
  m_Scene->BuildMeshEntityHierarchy(sampleEntity, exampleModel);
}

void Application::OnResize(int height, int width) {
  render->m_swapChainDesc.height = height;
  render->m_swapChainDesc.width = width;
  render->m_swapChain = render->BuildSwapChain(render->m_swapChainDesc, render->m_device, render->m_surface);
}

void MoveControls() {
  float speed = 1.1f;

  if (Keyboard::IsKeyPressing(Rain::Key::W)) {
    Player->ProcessKeyboard(FORWARD, speed);
  } else if (Keyboard::IsKeyPressing(Rain::Key::S)) {
    Player->ProcessKeyboard(BACKWARD, speed);
  } else if (Keyboard::IsKeyPressing(Rain::Key::A)) {
    Player->ProcessKeyboard(LEFT, speed);
  } else if (Keyboard::IsKeyPressing(Rain::Key::D)) {
    Player->ProcessKeyboard(RIGHT, speed);
  } else if (Keyboard::IsKeyPressing(Rain::Key::Space)) {
    Player->ProcessKeyboard(UP, speed);
  } else if (Keyboard::IsKeyPressing(Rain::Key::LeftShift)) {
    Player->ProcessKeyboard(DOWN, speed);
  } else {
    return;
  }
}

void Application::OnUpdate() {
  glfwPollEvents();
  MoveControls();

	// TODO: Temporary way to set camera until figure out scene-camera design
  m_Scene->m_SceneCamera = Player.get();
  m_Scene->OnUpdate();
  m_Scene->OnRender(m_Renderer);
}

void Application::OnMouseClick(Rain::MouseCode button) {
  // ImGuiIO& io = ImGui::GetIO();
  // if (io.WantCaptureMouse) {
  //   return;
  // }

  Cursor::CaptureMouse(true);
}

void Application::OnMouseMove(double xPos, double yPos) {
  static glm::vec2 prevCursorPos = Cursor::GetCursorPosition();

  if (!Cursor::IsMouseCaptured()) {
    return;
  }

  glm::vec2 cursorPos = Cursor::GetCursorPosition();

  Player->ProcessMouseMovement(cursorPos.x - prevCursorPos.x, cursorPos.y - prevCursorPos.y);

  prevCursorPos = cursorPos;
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
