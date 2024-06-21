#pragma once
#include <iostream>
#include "core/Assert.h"
#include "render/BindingManager.h"
#include "render/Shader.h"
#include "scene/Components.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "src/tint/lang/wgsl/ast/module.h"
#include "src/tint/lang/wgsl/sem/function.h"
#define RN_DEBUG
#include <GLFW/glfw3.h>
#include <unistd.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include "Application.h"

#include "Cam.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include "imgui.h"

#include "core/Log.h"
#include "io/cursor.h"
#include "io/filesystem.h"
#include "io/keyboard.h"
#include "render/GPUAllocator.h"
#include "render/Render.h"
#include "render/ResourceManager.h"

#include <tint/tint.h>
#include <iostream>

#if __EMSCRIPTEN__
#include <emscripten.h>
#include "platform/web/web_window.h"
#endif

extern "C" WGPUSurface glfwGetWGPUSurface(WGPUInstance instance, GLFWwindow* window);

std::unique_ptr<Render> render;
Application* Application::m_Instance;
std::shared_ptr<PlayerCamera> Player;

Scene* scene;

void Application::OnStart() {
  m_Instance = this;
  render = std::make_unique<Render>();
  scene = new Scene("Test Scene");

  Rain::Log::Init();

  WGPUInstance instance = render->CreateInstance();
  render->m_window = static_cast<GLFWwindow*>(GetNativeWindow());

#if __EMSCRIPTEN__
  render->m_surface = htmlGetCanvasSurface(instance, "canvas");
#else
  render->m_surface = glfwGetWGPUSurface(instance, render->m_window);
#endif

  if (render->m_surface == nullptr) {
    // RN_ERROR("Failed to create a rendering surface. The surface returned is null.");
    exit(-1);
  }

  render->Init(render->m_window, instance);
  GPUAllocator::Init();

  m_Renderer = CreateRef<SceneRenderer>();
  m_Renderer->Init();

  //Rain::ResourceManager::Init(std::make_shared<WGPUDevice>(render->m_device));
  Rain::ResourceManager::LoadTexture("T_Default", RESOURCE_DIR "/textures/placeholder.jpeg");

  Player = std::make_shared<PlayerCamera>();
  Player->Position.y = 0;
  Player->Position.z = 0;

  // Prep. ImgGui
  // =======================================================

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO();

  ImGui_ImplGlfw_InitForOther(render->m_window, true);

  ImGui_ImplWGPU_InitInfo initInfo;
  initInfo.Device = render->m_device;
  initInfo.RenderTargetFormat = render->m_swapChainFormat;
  initInfo.DepthStencilFormat = render->m_depthTextureFormat;
  ImGui_ImplWGPU_Init(&initInfo);

  // Prep. Scene & Systems
  // =======================================================

  Ref<MeshSource> boxModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/models/SponzaExp5.gltf");
  // Ref<MeshSource> boxModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/models/bonzer2.gltf");

  Entity entityBox = scene->CreateEntity("Box");
  entityBox.GetComponent<TransformComponent>()->Translation = glm::vec3(0, 10, 0);
  scene->BuildMeshEntityHierarchy(entityBox, boxModel);

  Cursor::Setup(render->m_window);
  Keyboard::Setup(render->m_window);
  // initBenchmark();

  Cursor::CaptureMouse(true);
}

void Application::OnResize(int height, int width) {
  render->m_swapChainDesc.height = height;
  render->m_swapChainDesc.width = width;

  render->m_swapChain = render->BuildSwapChain(render->m_swapChainDesc, render->m_device, render->m_surface);
  render->m_depthTexture = render->GetDepthBufferTexture(render->m_device, render->m_depthTextureFormat, render->m_swapChainDesc.width, render->m_swapChainDesc.height);
  render->m_depthTextureView = render->GetDepthBufferTextureView("T_DepthDefault", render->m_depthTexture, render->m_depthTextureFormat);
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

  scene->m_SceneCamera = Player.get();
  scene->OnUpdate();
  scene->OnRender(m_Renderer);
}

void Application::OnMouseClick(Rain::MouseCode button) {
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse) {
    return;
  }

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

std::string _labelPrefix(const char* const label) {
  float width = ImGui::CalcItemWidth();

  float x = ImGui::GetCursorPosX();
  ImGui::Text("%s", label);
  ImGui::SameLine();
  ImGui::SetCursorPosX(x + width * 0.5f + ImGui::GetStyle().ItemInnerSpacing.x);
  ImGui::SetNextItemWidth(-1);

  std::string labelID = "##";
  labelID += label;

  return labelID;
}

void Application::drawImgui(WGPURenderPassEncoder renderPass) {
  ImGui_ImplWGPU_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::Begin("Scene Settings");

  ImGuiIO& io = ImGui::GetIO();
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

  ImGui::Spacing();

  // physx::PxU32 version = PX_PHYSICS_VERSION;
  // physx::PxU32 major = (version >> 24) & 0xFF;
  // physx::PxU32 minor = (version >> 16) & 0xFF;
  // physx::PxU32 bugfix = (version >> 8) & 0xFF;

  // ImGui::Text("PhysX Version: %d.%d.%d", major, minor, bugfix);

  ImGui::Spacing();

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));  // RGB for green, with full opacity
  ImGui::Text("PRESS F TO SHOOT'EM UP");
  ImGui::PopStyleColor();
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.0f, 1.0f));  // RGB for green, with full opacity
  ImGui::Text("PRESS ESC TO UNLOCK MOUSE");
  ImGui::PopStyleColor();

  ImGui::End();

#ifdef RN_DEBUG
  ImGui::Begin("Statistics");
  ImGui::Text("Render pass duration on GPU: %s", m_perf.summary().c_str());
  // ImGui::Text("Physics simulation duration %.3f ms", simElapsed);
  ImGui::End();
#endif

  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Save Map")) {
      }
      if (ImGui::MenuItem("Load Map")) {
      }
      ImGui::EndMenu();
    }
  }
  ImGui::EndMainMenuBar();

  ImGui::EndFrame();
  ImGui::Render();
  ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
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
