#include "Application.h"
#include <GLFW/glfw3.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include "Model.h"
#include "io/cursor.h"
#include "io/keyboard.h"
#include "render/Camera.h"
#include "render/Render.h"
#include "render/ResourceManager.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include "imgui.h"
#include "scene/Scene.h"

#include <iostream>
#if __EMSCRIPTEN__
#include <emscripten.h>
#include "platform/web/web_window.h"
#endif
// #ifndef _DEBUG
#define _DEBUG

extern "C" WGPUSurface glfwGetWGPUSurface(WGPUInstance instance, GLFWwindow* window);

glm::vec3 cameraFront = glm::vec3(0.0f, -0.5f, 0);

float screenWidth = 1920;
float screenHeight = 1080;

bool switchKey = false;
GLFWwindow* m_window;
Render* render = new Render();

std::shared_ptr<PlayerCamera> Player;

Camera* Cam;
Model* sponza;

std::unique_ptr<PipelineManager> m_PipelineManager;
std::shared_ptr<ShaderManager> m_ShaderManager;
WGPURenderPipeline m_pipeline = nullptr;
WGPURenderPipeline m_pipeline_lit = nullptr;

WGPUSurface m_surface;

void Application::OnStart() {
  WGPUInstance instance = render->CreateInstance();
  m_window = static_cast<GLFWwindow*>(GetNativeWindow());

#if __EMSCRIPTEN__
  m_surface = htmlGetCanvasSurface(instance, "canvas");
#else
  m_surface = glfwGetWGPUSurface(instance, m_window);
#endif

  if (m_surface == nullptr) {
    std::cout << "An error occured surface is null" << std::endl;
  }

  render->Init(m_window, instance, m_surface);
  m_ShaderManager = std::make_shared<ShaderManager>(render->m_device);

  auto devicePtr = std::make_shared<WGPUDevice>(render->m_device);
  Rain::ResourceManager::Init(devicePtr);

  Rain::ResourceManager::LoadTexture("T_Box", RESOURCE_DIR "/wood.png");
  m_ShaderManager->LoadShader("SH_Default", RESOURCE_DIR "/shader_default.wgsl");
  m_PipelineManager = std::make_unique<PipelineManager>(render->m_device, m_ShaderManager);

  BufferLayout vertexLayout = {
      {ShaderDataType::Float3, "position"},
      {ShaderDataType::Float3, "normal"},
      {ShaderDataType::Float2, "uv"}};

  GroupLayout groupLayout = {
      {0, GroupLayoutVisibility::Both, GroupLayoutType::Default},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler},
      {1, GroupLayoutVisibility::Both, GroupLayoutType::Default}};

  m_pipeline = m_PipelineManager->CreatePipeline("RP_Default", "SH_Default", vertexLayout, groupLayout, m_surface, render->m_adapter);

  render->SetClearColor(0.52, 0.80, 0.92, 1);
	Cursor::Setup(m_window);
	Keyboard::Setup(m_window);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO();

  ImGui_ImplGlfw_InitForOther(m_window, true);

  ImGui_ImplWGPU_InitInfo initInfo;
  initInfo.Device = render->m_device;
  initInfo.RenderTargetFormat = render->m_swapChainFormat;
  initInfo.DepthStencilFormat = render->m_depthTextureFormat;

  ImGui_ImplWGPU_Init(&initInfo);

  glm::mat4 projectionMatrix = glm::perspective(75 * PI / 180, screenWidth / screenHeight, 0.1f, 2500.0f);

  WGPUBindGroupLayout cameraLayout = wgpuRenderPipelineGetBindGroupLayout(m_pipeline, 1);

	Player = std::make_shared<PlayerCamera>();
  Cam = new Camera(projectionMatrix, Player, render->m_device, cameraLayout);
  Cam->updateBuffer(render->m_queue);

  WGPUBindGroupLayout objectLayout = wgpuRenderPipelineGetBindGroupLayout(m_pipeline, 0);
  sponza = new Model(RESOURCE_DIR "/sponza.obj", objectLayout, render->m_device, render->m_queue, render->m_sampler);

	Cursor::CaptureMouse(true);
}

bool Application::isRunning() {
  return !glfwWindowShouldClose(m_window);
}

void Application::OnResize(int height, int width) {
  render->screenHeight = height;
  render->screenWidth = width;
  render->m_swapChainDesc.height = height;
  render->m_swapChainDesc.width = width;

  render->m_swapChain = render->buildSwapChain(render->m_swapChainDesc, render->m_device, m_surface);
  render->m_depthTexture = render->GetDepthBufferTexture(render->m_device, render->m_swapChainDesc);
  render->m_depthTextureView =
      render->GetDepthBufferTextureView(render->m_depthTexture, render->m_depthTextureFormat);
  float ratio = render->m_swapChainDesc.width / (float)render->m_swapChainDesc.height;

  Cam->uniform.projectionMatrix =
      glm::perspective(90 * PI / 180, ratio, 0.01f, 10000.0f);
  Cam->updateBuffer(render->m_queue);
}

void Application::OnMouseMove(double xPos, double yPos) {
  static glm::vec2 prevCursorPos = glm::vec2(0);

  glm::vec2 cursorPos = Cursor::GetCursorPosition();

	Player->ProcessMouseMovement(cursorPos.x - prevCursorPos.x, cursorPos.y - prevCursorPos.y);

  prevCursorPos = cursorPos;
	Cam->updateBuffer(render->m_queue);
}

void Application::OnMouseClick(Rain::MouseCode button) {
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse) {
    return;
  }
}

void MoveControls() {
	float mul = 5.0f;
	if(Keyboard::IsKeyPressing(Rain::Key::W))
	{
		Player->ProcessKeyboard(FORWARD, 1.0f * mul);
    Cam->updateBuffer(render->m_queue);
	}
	else if(Keyboard::IsKeyPressing(Rain::Key::S))
	{
		Player->ProcessKeyboard(BACKWARD, 1.0f * mul);
    Cam->updateBuffer(render->m_queue);
	}
	else if(Keyboard::IsKeyPressing(Rain::Key::A))
	{
		Player->ProcessKeyboard(LEFT, 1.0f * mul);
    Cam->updateBuffer(render->m_queue);
	}
	else if(Keyboard::IsKeyPressing(Rain::Key::D))
	{
		Player->ProcessKeyboard(RIGHT, 1.0f * mul);
    Cam->updateBuffer(render->m_queue);
	}
	else if(Keyboard::IsKeyPressing(Rain::Key::Space))
	{
		Player->ProcessKeyboard(UP, 1.0f * mul);
    Cam->updateBuffer(render->m_queue);
	}
	else if(Keyboard::IsKeyPressing(Rain::Key::LeftShift))
	{
		Player->ProcessKeyboard(DOWN, 1.0f * mul);
    Cam->updateBuffer(render->m_queue);
	}
}

void Application::OnUpdate() {
  glfwPollEvents();

  render->nextTexture = wgpuSwapChainGetCurrentTextureView(render->m_swapChain);
  if (!render->nextTexture) {
    fprintf(stderr, "Cannot acquire next swap chain texture\n");
    return;
  }

  WGPUCommandEncoderDescriptor commandEncoderDesc = {
      .label = "Command Encoder"};
  render->encoder = wgpuDeviceCreateCommandEncoder(render->m_device, &commandEncoderDesc);

  WGPURenderPassDescriptor renderPassDesc{};

  WGPURenderPassColorAttachment renderPassColorAttachment{};
  renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
  renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
  renderPassColorAttachment.clearValue = render->m_Color;
  renderPassDesc.colorAttachmentCount = 1;
  renderPassColorAttachment.resolveTarget = nullptr;
  renderPassColorAttachment.view = render->nextTexture;

#if !__EMSCRIPTEN__
  renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif
  renderPassDesc.colorAttachments = &renderPassColorAttachment;

  WGPURenderPassDepthStencilAttachment depthStencilAttachment;
  depthStencilAttachment.view = render->m_depthTextureView;
  depthStencilAttachment.depthClearValue = 1.0f;
  depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
  depthStencilAttachment.depthStoreOp = WGPUStoreOp_Store;
  depthStencilAttachment.depthReadOnly = false;
  depthStencilAttachment.stencilClearValue = 0;

  depthStencilAttachment.stencilLoadOp = WGPULoadOp_Undefined;
  depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Undefined;

  depthStencilAttachment.stencilReadOnly = true;

  renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

  renderPassDesc.timestampWrites = 0;
  renderPassDesc.timestampWrites = nullptr;

  render->renderPass = wgpuCommandEncoderBeginRenderPass(render->encoder, &renderPassDesc);

  wgpuRenderPassEncoderSetBindGroup(render->renderPass, 1, Cam->bindGroup, 0, NULL);

  sponza->Draw(render->renderPass, m_pipeline);
	MoveControls();

  updateGui(render->renderPass);

  wgpuRenderPassEncoderEnd(render->renderPass);
  wgpuTextureViewRelease(render->nextTexture);

  WGPUCommandBufferDescriptor cmdBufferDescriptor = {
      .label = "Command Buffer"};
  WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(render->encoder, &cmdBufferDescriptor);

  wgpuQueueSubmit(render->m_queue, 1, &commandBuffer);

#ifndef __EMSCRIPTEN__
  wgpuSwapChainPresent(render->m_swapChain);
#endif

#ifdef WEBGPU_BACKEND_DAWN
  wgpuDeviceTick(m_device);
#endif
}

void Application::OnKeyPressed(KeyCode key, KeyAction action) {
  if (key == Rain::Key::F && action == Rain::Key::RN_KEY_RELEASE) {
    if (!switchKey) {
      switchKey = true;
    }
  }
}

void Application::updateGui(WGPURenderPassEncoder renderPass) {
  ImGui_ImplWGPU_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::Begin("Hello, world!");

  ImGuiIO& io = ImGui::GetIO();
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
              1000.0f / io.Framerate, io.Framerate);
  ImGui::End();

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
