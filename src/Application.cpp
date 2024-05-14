#include "Application.h"
#include <GLFW/glfw3.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include "imgui.h"

#include "Constants.h"

#include "Model.h"
#include "core/Log.h"
#include "io/cursor.h"
#include "io/keyboard.h"
#include "render/Camera.h"
#include "render/PipelineManager.h"
#include "render/Render.h"
#include "render/ResourceManager.h"

#if __EMSCRIPTEN__
#include <emscripten.h>
#include "platform/web/web_window.h"
#endif
// #ifndef _DEBUG
#define _DEBUG

extern "C" WGPUSurface glfwGetWGPUSurface(WGPUInstance instance, GLFWwindow* window);

#define FOV 75.0f
#define PERSPECTIVE_NEAR 0.10f
#define PERSPECTIVE_FAR 1500.0f

std::unique_ptr<Render> render;

std::shared_ptr<PlayerCamera> Player;
std::shared_ptr<Camera> Cam;

Model* sponza;

std::unique_ptr<PipelineManager> m_PipelineManager;
std::shared_ptr<ShaderManager> m_ShaderManager;

WGPURenderPipeline m_pipeline = nullptr;
WGPURenderPipeline m_pipeline_ppfx = nullptr;
WGPUBindGroup m_bind_group_ppfx;

WGPUBuffer vertexBufferPpfx;
WGPUBuffer indexBufferPpfx;

WGPUTextureView offrenderTextureView;
WGPUSurface m_surface;

void ppfxCreateBindings(int width, int height) {
  WGPUTextureDescriptor textureDesc = {
      .usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
      .dimension = WGPUTextureDimension_2D,
      .size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
      .format = WGPUTextureFormat_BGRA8Unorm,
      .mipLevelCount = 1,
      .sampleCount = 1,
  };

  WGPUTexture intermediateTexture = wgpuDeviceCreateTexture(render->m_device, &textureDesc);
  offrenderTextureView = wgpuTextureCreateView(intermediateTexture, nullptr);

  static std::vector<WGPUBindGroupEntry> bindingsPpfx(2);

  bindingsPpfx[0].binding = 0;
  bindingsPpfx[0].textureView = offrenderTextureView;
  bindingsPpfx[0].offset = 0;

  bindingsPpfx[1].binding = 1;
  bindingsPpfx[1].sampler = render->m_sampler;
  bindingsPpfx[1].offset = 0;

  WGPUBindGroupDescriptor bindGroupDescPpfx = {};
  bindGroupDescPpfx.layout = wgpuRenderPipelineGetBindGroupLayout(m_pipeline_ppfx, 0);
  bindGroupDescPpfx.entryCount = (uint32_t)bindingsPpfx.size();
  bindGroupDescPpfx.entries = bindingsPpfx.data();

  m_bind_group_ppfx = wgpuDeviceCreateBindGroup(render->m_device, &bindGroupDescPpfx);
}

void Application::OnStart() {
  render = std::make_unique<Render>();

  WGPUInstance instance = render->CreateInstance();
  render->m_window = static_cast<GLFWwindow*>(GetNativeWindow());

#if __EMSCRIPTEN__
  m_surface = htmlGetCanvasSurface(instance, "canvas");
#else
  m_surface = glfwGetWGPUSurface(instance, render->m_window);
#endif

  if (m_surface == nullptr) {
    RN_ERROR("Failed to create a rendering surface. The surface returned is null.");
    exit(-1);
  }

  render->Init(render->m_window, instance, m_surface);

  Rain::ResourceManager::Init(std::make_shared<WGPUDevice>(render->m_device));
  Rain::ResourceManager::LoadTexture("T_Default", RESOURCE_DIR "/wood.png");

  m_ShaderManager = std::make_shared<ShaderManager>(render->m_device);
  m_ShaderManager->LoadShader("SH_Default", RESOURCE_DIR "/shader_default.wgsl");
  m_ShaderManager->LoadShader("SH_PPFX", RESOURCE_DIR "/shader_ppfx.wgsl");

  m_PipelineManager = std::make_unique<PipelineManager>(render->m_device, m_ShaderManager);


  // Prep. Offscreen Render Resources
  // =======================================================

  BufferLayout vertexLayout = {
      {ShaderDataType::Float3, "position"},
      {ShaderDataType::Float3, "normal"},
      {ShaderDataType::Float2, "uv"}};

  GroupLayout groupLayout = {
      {0, GroupLayoutVisibility::Both, GroupLayoutType::Default},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler},
      {1, GroupLayoutVisibility::Both, GroupLayoutType::Default}};

  m_pipeline = m_PipelineManager->CreatePipeline("RP_Default", "SH_Default", vertexLayout, groupLayout, render->m_depthTextureFormat, m_surface, render->m_adapter);

  WGPUBindGroupLayout cameraLayout = wgpuRenderPipelineGetBindGroupLayout(m_pipeline, 1);
  WGPUBindGroupLayout objectLayout = wgpuRenderPipelineGetBindGroupLayout(m_pipeline, 0);

  glm::mat4 projectionMatrix = glm::perspective(glm::radians(FOV), (float)render->m_swapChainDesc.width / render->m_swapChainDesc.height, PERSPECTIVE_NEAR, PERSPECTIVE_FAR);

  Player = std::make_shared<PlayerCamera>();
  Cam = std::make_shared<Camera>(projectionMatrix, Player, render->m_device, cameraLayout);
  Cam->updateBuffer(render->m_queue);

  // Prep. PPFX Resources
  // =======================================================

  BufferLayout vertexLayoutPpfx = {
      {ShaderDataType::Float3, "position"},
      {ShaderDataType::Float2, "uv"}};

  GroupLayout groupLayoutPpfx = {
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler}};

  m_pipeline_ppfx = m_PipelineManager->CreatePipeline("RP_PPFX", "SH_PPFX", vertexLayoutPpfx, groupLayoutPpfx, render->m_depthTextureFormat, m_surface, render->m_adapter);

  int screenWidth, screenHeight;
  glfwGetFramebufferSize((GLFWwindow*)render->m_window, &screenWidth, &screenHeight);
  ppfxCreateBindings(screenWidth, screenHeight);

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

  wgpuQueueWriteBuffer(render->m_queue, indexBufferPpfx, 0, quadIndices, sizeof(quadIndices));
  
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

  sponza = new Model(RESOURCE_DIR "/sponza.obj", objectLayout, render->m_device, render->m_queue, render->m_sampler);
  sponza->Transform.scale = glm::vec3(0.5f);
  sponza->UpdateUniforms(render->m_queue);

  Cursor::Setup(render->m_window);
  Keyboard::Setup(render->m_window);

  Cursor::CaptureMouse(true);
}

void Application::OnResize(int height, int width) {
  render->m_swapChainDesc.height = height;
  render->m_swapChainDesc.width = width;

  render->m_swapChain = render->buildSwapChain(render->m_swapChainDesc, render->m_device, m_surface);
  render->m_depthTexture = render->GetDepthBufferTexture(render->m_device, render->m_swapChainDesc);
  render->m_depthTextureView = render->GetDepthBufferTextureView(render->m_depthTexture, render->m_depthTextureFormat);

  ppfxCreateBindings(width, height);

  Cam->uniform.projectionMatrix = glm::perspective(glm::radians(FOV), (float)render->m_swapChainDesc.width / render->m_swapChainDesc.height, PERSPECTIVE_NEAR, PERSPECTIVE_FAR);
  Cam->updateBuffer(render->m_queue);
}

void MoveControls() {
  float speed = 5.0f;

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
  Cam->updateBuffer(render->m_queue);
}

void Application::OnUpdate() {
  glfwPollEvents();
  MoveControls();

  render->nextTexture = wgpuSwapChainGetCurrentTextureView(render->m_swapChain);

  if (!render->nextTexture) {
    RN_ERROR("Failed to acquire next swapchain texture.");
    return;
  }

  WGPUCommandEncoderDescriptor commandEncoderDesc = {.label = "Command Encoder"};
  render->encoder = wgpuDeviceCreateCommandEncoder(render->m_device, &commandEncoderDesc);

  WGPURenderPassDescriptor firstPassDesc{.label = "first_pass"};
  WGPURenderPassColorAttachment firstPassColorAttachment{};
  firstPassColorAttachment.loadOp = WGPULoadOp_Clear;
  firstPassColorAttachment.storeOp = WGPUStoreOp_Store;
  firstPassColorAttachment.clearValue = WGPUColor{0.52, 0.80, 0.92, 1};
  firstPassDesc.colorAttachmentCount = 1;
  firstPassColorAttachment.resolveTarget = nullptr;
  firstPassColorAttachment.view = offrenderTextureView;
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

  WGPURenderPassDescriptor secondPassDesc{.label = "second_pass"};
  WGPURenderPassColorAttachment secondPassColorAttachment = {};
  secondPassColorAttachment.loadOp = WGPULoadOp_Clear;
  secondPassColorAttachment.storeOp = WGPUStoreOp_Store;
  secondPassColorAttachment.clearValue = {0.0, 0.0, 0.0, 1.0};
  secondPassColorAttachment.view = render->nextTexture;
  secondPassDesc.colorAttachmentCount = 1;

#if !__EMSCRIPTEN__
  secondPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif
  secondPassDesc.colorAttachments = &secondPassColorAttachment;
  secondPassDesc.timestampWrites = 0;
  secondPassDesc.timestampWrites = nullptr;
  secondPassDesc.depthStencilAttachment = &firstDepthStencilAttachment;

  WGPURenderPassEncoder secondPassEncoder = wgpuCommandEncoderBeginRenderPass(render->encoder, &secondPassDesc);

  wgpuRenderPassEncoderSetPipeline(secondPassEncoder, m_pipeline_ppfx);
  wgpuRenderPassEncoderSetVertexBuffer(secondPassEncoder, 0, vertexBufferPpfx, 0, sizeof(quadVertices));
  wgpuRenderPassEncoderSetIndexBuffer(secondPassEncoder, indexBufferPpfx, WGPUIndexFormat_Uint32, 0, sizeof(quadIndices));
  wgpuRenderPassEncoderSetBindGroup(secondPassEncoder, 0, m_bind_group_ppfx, 0, nullptr);

  wgpuRenderPassEncoderDrawIndexed(secondPassEncoder, 6, 1, 0, 0, 0);
  drawImgui(secondPassEncoder);

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

void Application::OnMouseMove(double xPos, double yPos) { static glm::vec2 prevCursorPos = glm::vec2(0);

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

void Application::drawImgui(WGPURenderPassEncoder renderPass) {
  ImGui_ImplWGPU_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::Begin("Hello, world!");

  ImGuiIO& io = ImGui::GetIO();
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
              1000.0f / io.Framerate, io.Framerate);

  ImVec2 size = ImGui::GetWindowSize();
  // ImGui::Image((ImTextureID)intermediateTextureView, ImVec2(size.x, size.y));

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

bool Application::isRunning() {
  return !glfwWindowShouldClose(render->m_window);
}
