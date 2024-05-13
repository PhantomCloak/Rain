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

#include "Constants.h"
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

float screenWidth = 1920;
float screenHeight = 1080;

GLFWwindow* m_window;
Render* render = new Render();

std::shared_ptr<PlayerCamera> Player;

Camera* Cam;
Model* sponza;

std::unique_ptr<PipelineManager> m_PipelineManager;
std::shared_ptr<ShaderManager> m_ShaderManager;

WGPURenderPipeline m_pipeline = nullptr;
WGPURenderPipeline m_pipeline_ppfx = nullptr;
WGPUBindGroup m_bind_group_ppfx;
WGPUBuffer vertexBufferPpfx;
WGPUBuffer indexBufferPpfx;

WGPUTexture intermediateTexture;
WGPUTextureView intermediateTextureView;

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

  Rain::ResourceManager::Init(std::make_shared<WGPUDevice>(render->m_device));
  Rain::ResourceManager::LoadTexture("T_Box", RESOURCE_DIR "/wood.png");

  m_ShaderManager = std::make_shared<ShaderManager>(render->m_device);

  m_ShaderManager->LoadShader("SH_Default", RESOURCE_DIR "/shader_default.wgsl");
  m_ShaderManager->LoadShader("SH_PPFX", RESOURCE_DIR "/shader_ppfx.wgsl");

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

  BufferLayout vertexLayoutPpfx = {
      {ShaderDataType::Float3, "position"},
      {ShaderDataType::Float2, "uv"}};

  GroupLayout groupLayoutPpfx = {
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler}};

  m_pipeline_ppfx = m_PipelineManager->CreatePipeline("RP_PPFX", "SH_PPFX", vertexLayoutPpfx, groupLayoutPpfx, m_surface, render->m_adapter);

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

  WGPUBindGroupLayout cameraLayout = wgpuRenderPipelineGetBindGroupLayout(m_pipeline, 1);
  WGPUBindGroupLayout objectLayout = wgpuRenderPipelineGetBindGroupLayout(m_pipeline, 0);
  glm::mat4 projectionMatrix = glm::perspective(75 * PI / 180, screenWidth / screenHeight, 0.1f, 1500.0f);
  Player = std::make_shared<PlayerCamera>();
  Cam = new Camera(projectionMatrix, Player, render->m_device, cameraLayout);

  Cam->updateBuffer(render->m_queue);
  WGPUTextureDescriptor textureDesc = {
      .usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
      .size = {3840, 2160, 1},
      .format = WGPUTextureFormat_BGRA8Unorm,
	  .mipLevelCount = 1,
	  .sampleCount = 1,
  };

  intermediateTexture = wgpuDeviceCreateTexture(render->m_device, &textureDesc);
  intermediateTextureView = wgpuTextureCreateView(intermediateTexture, nullptr);

  static std::vector<WGPUBindGroupEntry> bindingsPpfx(2);

  bindingsPpfx[0].binding = 0;
  bindingsPpfx[0].textureView = intermediateTextureView;
  bindingsPpfx[0].offset = 0;

  bindingsPpfx[1].binding = 1;
  bindingsPpfx[1].sampler = render->m_sampler;
  bindingsPpfx[1].offset = 0;

  WGPUBindGroupDescriptor bindGroupDescPpfx = {};
  bindGroupDescPpfx.layout = wgpuRenderPipelineGetBindGroupLayout(m_pipeline_ppfx, 0);
  bindGroupDescPpfx.entryCount = (uint32_t)bindingsPpfx.size();
  bindGroupDescPpfx.entries = bindingsPpfx.data();

  m_bind_group_ppfx = wgpuDeviceCreateBindGroup(render->m_device, &bindGroupDescPpfx);
  int a = sizeof(quadVertices);

  WGPUBufferDescriptor vertexBufferDesc = {};
  vertexBufferDesc.size = sizeof(quadVertices);
  vertexBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
  vertexBufferDesc.mappedAtCreation = false;
  vertexBufferPpfx = wgpuDeviceCreateBuffer(render->m_device, &vertexBufferDesc);

  wgpuQueueWriteBuffer(render->m_queue, vertexBufferPpfx, 0, quadVertices, vertexBufferDesc.size);


  WGPUBufferDescriptor indexBufferDesc = {};
  indexBufferDesc.size = sizeof(quadIndices);  // Total size in bytes of the index data.
  indexBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
  indexBufferDesc.mappedAtCreation = false;
  indexBufferPpfx = wgpuDeviceCreateBuffer(render->m_device, &indexBufferDesc);

  // Upload index data to the buffer
  wgpuQueueWriteBuffer(render->m_queue, indexBufferPpfx, 0, quadIndices, sizeof(quadIndices));


  sponza = new Model(RESOURCE_DIR "/sponza.obj", objectLayout, render->m_device, render->m_queue, render->m_sampler);
  sponza->Transform.scale = glm::vec3(0.5f);
  sponza->UpdateUniforms(render->m_queue);

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

  // Cam->uniform.projectionMatrix =
  //     glm::perspective(90 * PI / 180, ratio, 0.01f, 10000.0f);
  // Cam->updateBuffer(render->m_queue);
}

void MoveControls() {
  float mul = 5.0f;

  if (Keyboard::IsKeyPressing(Rain::Key::W)) {
    Player->ProcessKeyboard(FORWARD, 1.0f * mul);
    Cam->updateBuffer(render->m_queue);
  } else if (Keyboard::IsKeyPressing(Rain::Key::S)) {
    Player->ProcessKeyboard(BACKWARD, 1.0f * mul);
    Cam->updateBuffer(render->m_queue);
  } else if (Keyboard::IsKeyPressing(Rain::Key::A)) {
    Player->ProcessKeyboard(LEFT, 1.0f * mul);
    Cam->updateBuffer(render->m_queue);
  } else if (Keyboard::IsKeyPressing(Rain::Key::D)) {
    Player->ProcessKeyboard(RIGHT, 1.0f * mul);
    Cam->updateBuffer(render->m_queue);
  } else if (Keyboard::IsKeyPressing(Rain::Key::Space)) {
    Player->ProcessKeyboard(UP, 1.0f * mul);
    Cam->updateBuffer(render->m_queue);
  } else if (Keyboard::IsKeyPressing(Rain::Key::LeftShift)) {
    Player->ProcessKeyboard(DOWN, 1.0f * mul);
    Cam->updateBuffer(render->m_queue);
  }
}

void Application::OnUpdate() {
  glfwPollEvents();
  MoveControls();

  render->nextTexture = wgpuSwapChainGetCurrentTextureView(render->m_swapChain);
  if (!render->nextTexture) {
    fprintf(stderr, "Cannot acquire next swap chain texture\n");
    return;
  }

  WGPUCommandEncoderDescriptor commandEncoderDesc = {.label = "Command Encoder"};
  render->encoder = wgpuDeviceCreateCommandEncoder(render->m_device, &commandEncoderDesc);

  WGPURenderPassDescriptor firstPassDesc{};
  WGPURenderPassColorAttachment firstPassColorAttachment{};
  firstPassColorAttachment.loadOp = WGPULoadOp_Clear;
  firstPassColorAttachment.storeOp = WGPUStoreOp_Store;
  firstPassColorAttachment.clearValue = render->m_Color;
  firstPassDesc.colorAttachmentCount = 1;
  firstPassColorAttachment.resolveTarget = nullptr;
  firstPassColorAttachment.view = intermediateTextureView;
  //firstPassColorAttachment.view = render->nextTexture;
#if !__EMSCRIPTEN__
  firstPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif
  firstPassDesc.colorAttachments = &firstPassColorAttachment;

  WGPURenderPassDepthStencilAttachment firstDepthStencilAttachment;
  firstDepthStencilAttachment.view = render->m_depthTextureView;
  firstDepthStencilAttachment.depthClearValue = 1.0f;
  firstDepthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
  firstDepthStencilAttachment.depthStoreOp = WGPUStoreOp_Store;
  firstDepthStencilAttachment.depthReadOnly = false;
  firstDepthStencilAttachment.stencilClearValue = 0;
  firstDepthStencilAttachment.stencilLoadOp = WGPULoadOp_Undefined;
  firstDepthStencilAttachment.stencilStoreOp = WGPUStoreOp_Undefined;
  firstDepthStencilAttachment.stencilReadOnly = true;

  firstPassDesc.depthStencilAttachment = &firstDepthStencilAttachment;
  firstPassDesc.timestampWrites = 0;
  firstPassDesc.timestampWrites = nullptr;

  WGPURenderPassEncoder firstPassEncoder = wgpuCommandEncoderBeginRenderPass(render->encoder, &firstPassDesc);

  wgpuRenderPassEncoderSetBindGroup(firstPassEncoder, 1, Cam->bindGroup, 0, NULL);

  sponza->Draw(firstPassEncoder, m_pipeline);

  wgpuRenderPassEncoderEnd(firstPassEncoder);

  WGPURenderPassDescriptor secondPassDesc{};
  WGPURenderPassColorAttachment secondPassColorAttachment = {};
  secondPassColorAttachment.loadOp = WGPULoadOp_Clear;
  secondPassColorAttachment.storeOp = WGPUStoreOp_Store;
  secondPassColorAttachment.clearValue = {0.0, 0.0, 0.0, 1.0};  // Clear to black
  secondPassColorAttachment.view = render->nextTexture;         // Output to the swap chain texture
  secondPassDesc.colorAttachmentCount = 1;

  secondPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
  secondPassDesc.colorAttachments = &secondPassColorAttachment;
  secondPassDesc.timestampWrites = 0;
  secondPassDesc.timestampWrites = nullptr;

  WGPURenderPassDepthStencilAttachment secondPassDepthAttachment;
  secondPassDepthAttachment.view = render->m_depthTextureView2;
  secondPassDepthAttachment.depthClearValue = 1.0f;
  secondPassDepthAttachment.depthLoadOp = WGPULoadOp_Clear;
  secondPassDepthAttachment.depthStoreOp = WGPUStoreOp_Store;
  secondPassDepthAttachment.depthReadOnly = false;
  secondPassDepthAttachment.stencilClearValue = 0;
  secondPassDepthAttachment.stencilLoadOp = WGPULoadOp_Undefined;
  secondPassDepthAttachment.stencilStoreOp = WGPUStoreOp_Undefined;
  secondPassDepthAttachment.stencilReadOnly = true;

  secondPassDesc.depthStencilAttachment = &secondPassDepthAttachment;

  WGPURenderPassEncoder secondPassEncoder = wgpuCommandEncoderBeginRenderPass(render->encoder, &secondPassDesc);

  wgpuRenderPassEncoderSetPipeline(secondPassEncoder, m_pipeline);
  wgpuRenderPassEncoderSetVertexBuffer(secondPassEncoder, 0, vertexBufferPpfx, 0, sizeof(quadVertices));
  wgpuRenderPassEncoderSetIndexBuffer(secondPassEncoder, indexBufferPpfx, WGPUIndexFormat_Uint32, 0, sizeof(quadIndices));
  wgpuRenderPassEncoderSetPipeline(secondPassEncoder, m_pipeline_ppfx);
  wgpuRenderPassEncoderSetBindGroup(secondPassEncoder, 0, m_bind_group_ppfx, 0, nullptr);

  //wgpuRenderPassEncoderDraw(secondPassEncoder, 4, 1, 0, 0);
  wgpuRenderPassEncoderDrawIndexed(secondPassEncoder, 6, 1, 0, 0, 0);

  //updateGui(secondPassEncoder);

  wgpuRenderPassEncoderEnd(secondPassEncoder);

  wgpuTextureViewRelease(render->nextTexture);

  WGPUCommandBufferDescriptor cmdBufferDescriptor = {.label = "Command Buffer"};
  WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(render->encoder, &cmdBufferDescriptor);

  wgpuQueueSubmit(render->m_queue, 1, &commandBuffer);

#ifndef __EMSCRIPTEN__
  wgpuSwapChainPresent(render->m_swapChain);
#endif

#ifdef WEBGPU_BACKEND_DAWN
  wgpuDeviceTick(m_device);
#endif
}

void Application::OnMouseClick(Rain::MouseCode button) {
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse) {
    return;
  }

  Cursor::CaptureMouse(true);
}

void Application::OnMouseMove(double xPos, double yPos) {
  static glm::vec2 prevCursorPos = glm::vec2(0);

  if (!Cursor::IsMouseCaptured()) {
    return;
  }

  glm::vec2 cursorPos = Cursor::GetCursorPosition();

  Player->ProcessMouseMovement(cursorPos.x - prevCursorPos.x, cursorPos.y - prevCursorPos.y);

  prevCursorPos = cursorPos;
  Cam->updateBuffer(render->m_queue);
}

void Application::OnKeyPressed(KeyCode key, KeyAction action) {
  if (key == Rain::Key::Escape && action == Rain::Key::RN_KEY_RELEASE) {
    Cursor::CaptureMouse(false);
  }
}

void Application::updateGui(WGPURenderPassEncoder renderPass) {
  ImGui_ImplWGPU_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::Begin("Hello, world!");

  ImGuiIO& io = ImGui::GetIO();
  //ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
  //            1000.0f / io.Framerate, io.Framerate);

  ImVec2 size = ImGui::GetWindowSize();
  ImGui::Image((ImTextureID)intermediateTextureView, ImVec2(size.x, size.y));

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
