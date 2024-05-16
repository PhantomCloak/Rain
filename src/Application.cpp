#include "Application.h"
#include <GLFW/glfw3.h>
#include <unistd.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <iostream>

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
#define PERSPECTIVE_FAR 2500.0f

//#define SHADOW_WIDTH 4098.0f
//#define SHADOW_HEIGHT 4098.0f

#define SHADOW_WIDTH 3840.0f
#define SHADOW_HEIGHT 2160.0f

#define SHADOW_NEAR 0.10f
#define SHADOW_FAR 2500.0f

struct ShadowUniform {
  glm::mat4x4 lightProjection;
  glm::mat4x4 lightView;
  glm::vec3 lightPos;
	int padding;
};

struct DebugUniform {
	float near;
	float far;
};

std::unique_ptr<Render> render;

std::shared_ptr<PlayerCamera> Player;
std::shared_ptr<Camera> Cam;
std::shared_ptr<Camera> ShadowCamera;

Model* sponza;

std::unique_ptr<PipelineManager> m_PipelineManager;
std::shared_ptr<ShaderManager> m_ShaderManager;

WGPURenderPipeline m_pipeline = nullptr;
WGPURenderPipeline m_pipeline_shadow = nullptr;
WGPURenderPipeline m_pipeline_ppfx = nullptr;
WGPURenderPipeline m_pipeline_debug = nullptr;

WGPUBindGroup m_bind_group_ppfx;
WGPUBindGroup m_bind_group_debug;
WGPUBindGroup m_bind_group_shadow_map;

WGPUBuffer vertexBufferPpfx;
WGPUBuffer indexBufferPpfx;

WGPUBuffer shadowUniformBuffer;
ShadowUniform shadowUniform;

WGPUTextureView offrenderOutTextureView;
WGPUTextureView shadowOutTextureView;
WGPUTextureView debugOutTextureView;

WGPUTexture shadowDepthTexture;
WGPUTextureView shadowDepthView;

WGPUSurface m_surface;

glm::vec3 shadowPos = glm::vec3(0);
glm::vec3 shadowRot = glm::vec3(0);

glm::mat4 GetViewMatrix(glm::vec3 pos, glm::vec3 rot) {
  glm::vec3 shadowCameraPos = pos;
  glm::vec3 shadowCameraRot = rot;

  float pitch = shadowCameraRot.x;
  float yaw = shadowCameraRot.y;
  float roll = shadowCameraRot.z;

  glm::vec3 front;
  front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
  front.y = sin(glm::radians(pitch));
  front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

  glm::vec3 Front = glm::normalize(front);
  glm::vec3 worldUp = glm::vec3(0, 1, 0);
  glm::vec3 right = glm::normalize(glm::cross(Front, worldUp));
  glm::vec3 up = glm::rotate(glm::mat4(1.0f), glm::radians(roll), right) * glm::vec4(worldUp, 1.0);

  return glm::lookAt(shadowCameraPos, shadowCameraPos + Front, up);
}

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
  offrenderOutTextureView = wgpuTextureCreateView(intermediateTexture, nullptr);

  static std::vector<WGPUBindGroupEntry> bindingsPpfx(2);

  bindingsPpfx[0].binding = 0;
  bindingsPpfx[0].textureView = offrenderOutTextureView;
  bindingsPpfx[0].offset = 0;

  bindingsPpfx[1].binding = 1;
  bindingsPpfx[1].sampler = render->m_sampler;
  bindingsPpfx[1].offset = 0;

  WGPUBindGroupDescriptor bindGroupDescPpfx = {.label = "bg_ppfx"};
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
  m_ShaderManager->LoadShader("SH_Shadow", RESOURCE_DIR "/shadow.wgsl");
  m_ShaderManager->LoadShader("SH_Debug", RESOURCE_DIR "/debug.wgsl");

  m_PipelineManager = std::make_unique<PipelineManager>(render->m_device, m_ShaderManager);

  int screenWidth, screenHeight;
  glfwGetFramebufferSize((GLFWwindow*)render->m_window, &screenWidth, &screenHeight);

  // Shared
  // =======================================================

  BufferLayout vertexLayoutDefault = {
      {ShaderDataType::Float3, "position"},
      {ShaderDataType::Float3, "normal"},
      {ShaderDataType::Float2, "uv"}};

  // Prep. Shadow Resources
  // =======================================================

  float shadowWidth = SHADOW_WIDTH;
  float shadowHeight = SHADOW_HEIGHT;

  // Let be it for now
  GroupLayout groupLayoutShadow = {
      {0, GroupLayoutVisibility::Both, GroupLayoutType::Default},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler},
      {1, GroupLayoutVisibility::Both, GroupLayoutType::Default}};

  shadowDepthTexture = render->GetDepthBufferTexture(render->m_device, render->m_depthTextureFormat, shadowWidth, shadowHeight, true);
  shadowDepthView = render->GetDepthBufferTextureView("T_Depth_Shadow", shadowDepthTexture, render->m_depthTextureFormat);

  m_pipeline_shadow = m_PipelineManager->CreatePipeline("RP_Shadow",
                                                        "SH_Shadow",
                                                        vertexLayoutDefault,
                                                        groupLayoutShadow,
                                                        render->m_depthTextureFormat,
                                                        render->m_swapChainFormat,
                                                        m_surface,
                                                        render->m_adapter);

  //glm::vec3 shadowCameraPos = glm::vec3(0, 3000, 0);
  //glm::vec3 shadowCameraRot = glm::vec3(-90, 0, 0);
  //glm::vec3 shadowCameraRot = glm::vec3(-66, 28, 0);

  shadowPos = glm::vec3(-282, 2346, -65);
	shadowRot = glm::vec3(-66, 28, 0);

  glm::mat4x4 shadowCameraView = GetViewMatrix(shadowPos, shadowRot);
  glm::mat4 shadowProjectionMatrix = glm::ortho(-shadowWidth / 2, shadowWidth / 2, -shadowHeight / 2, shadowHeight / 2,
                                                SHADOW_NEAR,
                                                SHADOW_FAR);

  WGPUBindGroupLayout objectLayout = wgpuRenderPipelineGetBindGroupLayout(m_pipeline_shadow, 0);
  WGPUBindGroupLayout cameraLayout = wgpuRenderPipelineGetBindGroupLayout(m_pipeline_shadow, 1);

  ShadowCamera = std::make_shared<Camera>(shadowProjectionMatrix, shadowCameraView, render->m_device, cameraLayout);
  ShadowCamera->updateBufferStandalone(render->m_queue);


  WGPUTextureDescriptor textureDescShadow = {
      .usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
      .dimension = WGPUTextureDimension_2D,
      .size = {static_cast<uint32_t>(screenWidth), static_cast<uint32_t>(screenHeight), 1},
      .format = WGPUTextureFormat_BGRA8Unorm,
      .mipLevelCount = 1,
      .sampleCount = 1,
  };

  WGPUTexture intermediateTextureShadow = wgpuDeviceCreateTexture(render->m_device, &textureDescShadow);
  shadowOutTextureView = wgpuTextureCreateView(intermediateTextureShadow, nullptr);

  // Prep. Offscreen Render Resources
  // =======================================================

  GroupLayout groupLayoutDefault = {
      {0, GroupLayoutVisibility::Both, GroupLayoutType::Default},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler},
      {1, GroupLayoutVisibility::Both, GroupLayoutType::Default},
      {2, GroupLayoutVisibility::Both, GroupLayoutType::TextureDepth},
      {2, GroupLayoutVisibility::Both, GroupLayoutType::SamplerCompare},
      {2, GroupLayoutVisibility::Both, GroupLayoutType::Default}};

  m_pipeline = m_PipelineManager->CreatePipeline("RP_Default",
                                                 "SH_Default",
                                                 vertexLayoutDefault,
                                                 groupLayoutDefault,
                                                 render->m_depthTextureFormat,
                                                 render->m_swapChainFormat,
                                                 m_surface,
                                                 render->m_adapter);

  WGPUSamplerDescriptor shadowSamplerDesc = {};  // Zero-initialize the struct

  // Set the properties
  shadowSamplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
  shadowSamplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
  shadowSamplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;

  shadowSamplerDesc.minFilter = WGPUFilterMode_Nearest;
  shadowSamplerDesc.magFilter = WGPUFilterMode_Linear;

  shadowSamplerDesc.compare = WGPUCompareFunction_Less;
  shadowSamplerDesc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
	shadowSamplerDesc.compare = WGPUCompareFunction_Less,
  shadowSamplerDesc.lodMinClamp = 0.0f;
  shadowSamplerDesc.lodMaxClamp = 1.0f;
  shadowSamplerDesc.maxAnisotropy = 1;

  WGPUSampler shadowSampler = wgpuDeviceCreateSampler(render->m_device, &shadowSamplerDesc);

  static WGPUBufferDescriptor uniformBufferDescShadow = {};
  uniformBufferDescShadow.size = sizeof(ShadowUniform);
  uniformBufferDescShadow.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
  uniformBufferDescShadow.mappedAtCreation = false;

  shadowUniformBuffer = wgpuDeviceCreateBuffer(render->m_device, &uniformBufferDescShadow);

  shadowUniform.lightProjection = shadowProjectionMatrix;
  shadowUniform.lightView = shadowCameraView;
  shadowUniform.lightPos = shadowPos;

  wgpuQueueWriteBuffer(render->m_queue, shadowUniformBuffer, 0, &shadowUniform, sizeof(ShadowUniform));

  WGPUBindGroupLayout shadowLayout = wgpuRenderPipelineGetBindGroupLayout(m_pipeline, 2);
  static std::vector<WGPUBindGroupEntry> bindingsShadowMap(3);

  bindingsShadowMap[0].binding = 0;
  bindingsShadowMap[0].offset = 0;
  bindingsShadowMap[0].textureView = shadowDepthView;

  bindingsShadowMap[1].binding = 1;
  bindingsShadowMap[1].offset = 0;
  bindingsShadowMap[1].sampler = shadowSampler;

  bindingsShadowMap[2].binding = 2;
  bindingsShadowMap[2].offset = 0;
  bindingsShadowMap[2].buffer = shadowUniformBuffer;
  bindingsShadowMap[2].size = sizeof(ShadowUniform);

  WGPUBindGroupDescriptor bindGroupDescShadowMap = {.label = "bg_shadowmap"};
  bindGroupDescShadowMap.layout = shadowLayout;
  bindGroupDescShadowMap.entryCount = (uint32_t)bindingsShadowMap.size();
  bindGroupDescShadowMap.entries = bindingsShadowMap.data();

  m_bind_group_shadow_map = wgpuDeviceCreateBindGroup(render->m_device, &bindGroupDescShadowMap);

  glm::mat4 projectionMatrix = glm::perspective(glm::radians(FOV), (float)render->m_swapChainDesc.width / render->m_swapChainDesc.height, PERSPECTIVE_NEAR, PERSPECTIVE_FAR);

  Player = std::make_shared<PlayerCamera>();
  Cam = std::make_shared<Camera>(projectionMatrix, Player, render->m_device, cameraLayout);
  Cam->updateBuffer(render->m_queue);

  // Prep. Debug Resources
  // =======================================================

  BufferLayout vertexLayoutDebug = {
      {ShaderDataType::Float3, "position"},
      {ShaderDataType::Float2, "uv"}};

  GroupLayout groupLayoutDebug = {
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::TextureDepth},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Default}};

  static std::vector<WGPUBindGroupEntry> bindingsDebug(3);

  static DebugUniform dbgUniform = {
	  .near = SHADOW_NEAR,
	  .far = SHADOW_FAR
  };

  static WGPUBufferDescriptor uniformBufferDebugDesc = {};
  uniformBufferDebugDesc.size = sizeof(DebugUniform);
  uniformBufferDebugDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
  uniformBufferDebugDesc.mappedAtCreation = false;

  static WGPUBuffer dbgUniformBuffer;
  dbgUniformBuffer = wgpuDeviceCreateBuffer(render->m_device, &uniformBufferDebugDesc);
  wgpuQueueWriteBuffer(render->m_queue, dbgUniformBuffer, 0, &dbgUniform, sizeof(DebugUniform));

  bindingsDebug[0].binding = 0;
  bindingsDebug[0].textureView = shadowDepthView;
  bindingsDebug[0].offset = 0;

  bindingsDebug[1].binding = 1;
  bindingsDebug[1].sampler = render->m_sampler;
  bindingsDebug[1].offset = 0;

  bindingsDebug[2].binding = 2;
  bindingsDebug[2].buffer = dbgUniformBuffer;
  bindingsDebug[2].size = sizeof(DebugUniform);
  bindingsDebug[2].offset = 0;

  m_pipeline_debug = m_PipelineManager->CreatePipeline("RP_Debug",
                                                       "SH_Debug",
                                                       vertexLayoutDebug,
                                                       groupLayoutDebug,
                                                       WGPUTextureFormat_Undefined,
                                                       render->m_swapChainFormat,
                                                       m_surface,
                                                       render->m_adapter);

  WGPUBindGroupDescriptor bindGroupDescDebug = {.label = "bg_debug"};
  bindGroupDescDebug.layout = wgpuRenderPipelineGetBindGroupLayout(m_pipeline_debug, 0);
  bindGroupDescDebug.entryCount = (uint32_t)bindingsDebug.size();
  bindGroupDescDebug.entries = bindingsDebug.data();

  m_bind_group_debug = wgpuDeviceCreateBindGroup(render->m_device, &bindGroupDescDebug);

  WGPUTextureDescriptor textureDescDebug = {
      .usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
      .dimension = WGPUTextureDimension_2D,
      .size = {static_cast<uint32_t>(screenWidth), static_cast<uint32_t>(screenHeight), 1},
      .format = WGPUTextureFormat_BGRA8Unorm,
      .mipLevelCount = 1,
      .sampleCount = 1,
  };

  WGPUTexture intermediateTextureDebug = wgpuDeviceCreateTexture(render->m_device, &textureDescDebug);
  debugOutTextureView = wgpuTextureCreateView(intermediateTextureDebug, nullptr);

  // Prep. PPFX Resources
  // =======================================================

  BufferLayout vertexLayoutPpfx = {
      {ShaderDataType::Float3, "position"},
      {ShaderDataType::Float2, "uv"}};

  GroupLayout groupLayoutPpfx = {
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler}};

  m_pipeline_ppfx = m_PipelineManager->CreatePipeline("RP_PPFX",
                                                      "SH_PPFX",
                                                      vertexLayoutPpfx,
                                                      groupLayoutPpfx,
                                                      render->m_depthTextureFormat,
                                                      render->m_swapChainFormat,
                                                      m_surface,
                                                      render->m_adapter);

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
  std::cout << "resize" << std::endl;
  render->m_swapChainDesc.height = height;
  render->m_swapChainDesc.width = width;

  render->m_swapChain = render->buildSwapChain(render->m_swapChainDesc, render->m_device, m_surface);
  render->m_depthTexture = render->GetDepthBufferTexture(render->m_device, render->m_depthTextureFormat, render->m_swapChainDesc.width, render->m_swapChainDesc.height);
  render->m_depthTextureView = render->GetDepthBufferTextureView("T_DepthDefault", render->m_depthTexture, render->m_depthTextureFormat);

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

  // Shadow Pass
  // =======================================================

  WGPURenderPassColorAttachment shadowColorAttachment = {};
  shadowColorAttachment.loadOp = WGPULoadOp_Clear;
  shadowColorAttachment.storeOp = WGPUStoreOp_Store;
  shadowColorAttachment.clearValue = {0.0, 0.0, 0.0, 1.0};
  shadowColorAttachment.view = shadowOutTextureView;
#if !__EMSCRIPTEN__
  shadowColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

  WGPURenderPassDepthStencilAttachment shadowDepthAttachment;
  shadowDepthAttachment.view = shadowDepthView;
  shadowDepthAttachment.depthClearValue = 1.0f;
  shadowDepthAttachment.depthLoadOp = WGPULoadOp_Clear;
  shadowDepthAttachment.depthStoreOp = WGPUStoreOp_Store;
  shadowDepthAttachment.depthReadOnly = false;
  shadowDepthAttachment.stencilClearValue = 0;
  shadowDepthAttachment.stencilLoadOp = WGPULoadOp_Undefined;
  shadowDepthAttachment.stencilStoreOp = WGPUStoreOp_Undefined;
  shadowDepthAttachment.stencilReadOnly = true;

  WGPURenderPassDescriptor shadowPassDesc{.label = "shadow_pass"};
  shadowPassDesc.timestampWrites = 0;
  shadowPassDesc.timestampWrites = nullptr;
  shadowPassDesc.colorAttachments = nullptr;

  shadowPassDesc.colorAttachmentCount = 1;
  shadowPassDesc.depthStencilAttachment = &shadowDepthAttachment;
  shadowPassDesc.colorAttachments = &shadowColorAttachment;

  WGPURenderPassEncoder shadowPassEncoder = wgpuCommandEncoderBeginRenderPass(render->encoder, &shadowPassDesc);

  ShadowCamera->uniform.viewMatrix = shadowUniform.lightView;
  ShadowCamera->uniform.projectionMatrix = shadowUniform.lightProjection;
  ShadowCamera->updateBufferStandalone(render->m_queue);

  wgpuRenderPassEncoderSetBindGroup(shadowPassEncoder, 1, ShadowCamera->bindGroup, 0, NULL);
  wgpuRenderPassEncoderSetViewport(shadowPassEncoder, 0, 0, SHADOW_WIDTH, SHADOW_HEIGHT, 0, 1);

  sponza->Draw(shadowPassEncoder, m_pipeline_shadow);

  wgpuRenderPassEncoderEnd(shadowPassEncoder);

  // Debug Pass
  // =======================================================

  WGPURenderPassColorAttachment debugColorAttachment = {};
  debugColorAttachment.loadOp = WGPULoadOp_Clear;
  debugColorAttachment.storeOp = WGPUStoreOp_Store;
  debugColorAttachment.clearValue = {0.0, 0.0, 0.0, 1.0};
  debugColorAttachment.view = debugOutTextureView;
#if !__EMSCRIPTEN__
  debugColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

  WGPURenderPassDescriptor debugPassDesc{.label = "debug_pass"};
  debugPassDesc.colorAttachmentCount = 1;
  debugPassDesc.timestampWrites = 0;
  debugPassDesc.timestampWrites = nullptr;
  debugPassDesc.colorAttachments = &debugColorAttachment;

  WGPURenderPassEncoder debugPassEncoder = wgpuCommandEncoderBeginRenderPass(render->encoder, &debugPassDesc);

  wgpuRenderPassEncoderSetPipeline(debugPassEncoder, m_pipeline_debug);
  wgpuRenderPassEncoderSetVertexBuffer(debugPassEncoder, 0, vertexBufferPpfx, 0, sizeof(quadVertices));
  wgpuRenderPassEncoderSetIndexBuffer(debugPassEncoder, indexBufferPpfx, WGPUIndexFormat_Uint32, 0, sizeof(quadIndices));
  wgpuRenderPassEncoderSetBindGroup(debugPassEncoder, 0, m_bind_group_debug, 0, nullptr);

  wgpuRenderPassEncoderDrawIndexed(debugPassEncoder, 6, 1, 0, 0, 0);

  wgpuRenderPassEncoderEnd(debugPassEncoder);

  // Lit Pass
  // =======================================================

  WGPURenderPassColorAttachment litPassColorAttachment{};
  litPassColorAttachment.loadOp = WGPULoadOp_Clear;
  litPassColorAttachment.storeOp = WGPUStoreOp_Store;
  litPassColorAttachment.clearValue = WGPUColor{0.52, 0.80, 0.92, 1};
  litPassColorAttachment.resolveTarget = nullptr;
  litPassColorAttachment.view = offrenderOutTextureView;
#if !__EMSCRIPTEN__
  litPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

  WGPURenderPassDepthStencilAttachment litDepthAttachment;
  litDepthAttachment.view = render->m_depthTextureView;
  litDepthAttachment.depthClearValue = 1.0f;
  litDepthAttachment.depthLoadOp = WGPULoadOp_Clear;
  litDepthAttachment.depthStoreOp = WGPUStoreOp_Store;
  litDepthAttachment.depthReadOnly = false;
  litDepthAttachment.stencilClearValue = 0;
  litDepthAttachment.stencilLoadOp = WGPULoadOp_Undefined;
  litDepthAttachment.stencilStoreOp = WGPUStoreOp_Undefined;
  litDepthAttachment.stencilReadOnly = true;

  WGPURenderPassDescriptor litPassDesc{.label = "first_pass"};
  litPassDesc.colorAttachmentCount = 1;
  litPassDesc.timestampWrites = 0;
  litPassDesc.timestampWrites = nullptr;
  litPassDesc.colorAttachments = &litPassColorAttachment;
  litPassDesc.depthStencilAttachment = &litDepthAttachment;

  WGPURenderPassEncoder litPassEncoder = wgpuCommandEncoderBeginRenderPass(render->encoder, &litPassDesc);

  wgpuRenderPassEncoderSetBindGroup(litPassEncoder, 1, Cam->bindGroup, 0, NULL);
  wgpuRenderPassEncoderSetBindGroup(litPassEncoder, 2, m_bind_group_shadow_map, 0, NULL);

  sponza->Draw(litPassEncoder, m_pipeline);

  wgpuRenderPassEncoderEnd(litPassEncoder);

  // PPFX Pass
  // =======================================================

  WGPURenderPassColorAttachment ppfxPassColorAttachment = {};
  ppfxPassColorAttachment.loadOp = WGPULoadOp_Clear;
  ppfxPassColorAttachment.storeOp = WGPUStoreOp_Store;
  ppfxPassColorAttachment.clearValue = {0.0, 0.0, 0.0, 1.0};
  ppfxPassColorAttachment.view = render->nextTexture;
#if !__EMSCRIPTEN__
  ppfxPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

  WGPURenderPassDescriptor ppfxPassDesc{.label = "second_pass"};
  ppfxPassDesc.colorAttachmentCount = 1;
  ppfxPassDesc.timestampWrites = 0;
  ppfxPassDesc.timestampWrites = nullptr;
  ppfxPassDesc.colorAttachments = &ppfxPassColorAttachment;
  ppfxPassDesc.depthStencilAttachment = &litDepthAttachment;

  WGPURenderPassEncoder ppfxPassEncoder = wgpuCommandEncoderBeginRenderPass(render->encoder, &ppfxPassDesc);

  wgpuRenderPassEncoderSetPipeline(ppfxPassEncoder, m_pipeline_ppfx);
  wgpuRenderPassEncoderSetVertexBuffer(ppfxPassEncoder, 0, vertexBufferPpfx, 0, sizeof(quadVertices));
  wgpuRenderPassEncoderSetIndexBuffer(ppfxPassEncoder, indexBufferPpfx, WGPUIndexFormat_Uint32, 0, sizeof(quadIndices));
  wgpuRenderPassEncoderSetBindGroup(ppfxPassEncoder, 0, m_bind_group_ppfx, 0, nullptr);

  wgpuRenderPassEncoderDrawIndexed(ppfxPassEncoder, 6, 1, 0, 0, 0);
  drawImgui(ppfxPassEncoder);

  wgpuRenderPassEncoderEnd(ppfxPassEncoder);

  // Submit
  // =======================================================

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

void Application::drawImgui(WGPURenderPassEncoder renderPass) {
  ImGui_ImplWGPU_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::Begin("Hello, world!");

	bool isUpdate = false;

	if(ImGui::InputFloat("Shadow Pos X", &shadowPos.x, 1.0f))
	{
		isUpdate = true;
	}
	if(ImGui::InputFloat("Shadow Pos Y", &shadowPos.y, 1.0f))
	{
		isUpdate = true;
	}
	if(ImGui::InputFloat("Shadow Pos Z", &shadowPos.z, 1.0f))
	{
		isUpdate = true;
	}


	if(ImGui::InputFloat("Shadow Rot X", &shadowRot.x, 0.5f))
	{
		isUpdate = true;
	}
	if(ImGui::InputFloat("Shadow Rot Y", &shadowRot.y, 0.5f))
	{
		isUpdate = true;
	}
	if(ImGui::InputFloat("Shadow Rot Z", &shadowRot.z, 0.5f))
	{
		isUpdate = true;
	}

	if(isUpdate)
	{
		shadowUniform.lightPos = shadowPos;
		shadowUniform.lightView = GetViewMatrix(shadowPos, shadowRot);

		ShadowCamera->uniform.viewMatrix = GetViewMatrix(shadowPos, shadowRot);

		wgpuQueueWriteBuffer(render->m_queue, shadowUniformBuffer, 0, &shadowUniform, sizeof(ShadowUniform));
		ShadowCamera->updateBufferStandalone(render->m_queue);
	}

	ImGui::End();

  ImGui::Begin("Hello, world!");

  ImGuiIO& io = ImGui::GetIO();
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
              1000.0f / io.Framerate, io.Framerate);

  ImVec2 size = ImGui::GetWindowSize();
  ImGui::Image((ImTextureID)shadowOutTextureView, ImVec2(size.x, size.y));

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
