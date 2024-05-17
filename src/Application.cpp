#include "Application.h"
#include <GLFW/glfw3.h>
#include <unistd.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <iostream>

#include "Cam.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include "imgui.h"

#include "Constants.h"

#include "Model.h"
#include "core/Log.h"
#include "io/cursor.h"
#include "io/keyboard.h"
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

#define SHADOW_WIDTH 2048.0f
#define SHADOW_HEIGHT 2048.0f

#define SHADOW_NEAR 0.10f
#define SHADOW_FAR 2500.0f

struct CameraUniform {
  glm::mat4x4 projectionMatrix;
  glm::mat4x4 viewMatrix;
};

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

Model* sponza;

std::unique_ptr<PipelineManager> pipelineManager;
std::shared_ptr<ShaderManager> shaderManager;

WGPURenderPipeline pipelineDefault = nullptr;
WGPURenderPipeline pipelineShadow = nullptr;
WGPURenderPipeline m_pipeline_ppfx = nullptr;
WGPURenderPipeline pipelineDebug = nullptr;

WGPUBindGroup bg_bgPpfx;
WGPUBindGroup bgDebug;
WGPUBindGroup bgShadowMap;

WGPUBindGroup bgCameraDefault;
WGPUBindGroup bgCameraShadow;

WGPUBuffer vertexBufferPpfx;
WGPUBuffer indexBufferPpfx;

ShadowUniform shadowUniform;
WGPUBuffer shadowUniformBuffer;

CameraUniform defaultCameraUniform;
WGPUBuffer defaultCameraUniformBuffer;

CameraUniform shadowCameraUniform;
WGPUBuffer shadowCameraUniformBuffer;

WGPUTextureView offrenderOutTextureView;
WGPUTextureView debugOutTextureView;
WGPUTextureView shadowDepthView;

glm::vec3 shadowPos = glm::vec3(0);
glm::vec3 shadowRot = glm::vec3(0);

glm::mat4 defaultProjection;
glm::mat4 defaultView;

glm::mat4 shadowProjection;
glm::mat4 shadowView;

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
  if (offrenderOutTextureView != NULL) {
    wgpuTextureViewRelease(offrenderOutTextureView);
  }

  WGPUTextureDescriptor textureDesc = {
      .usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
      .dimension = WGPUTextureDimension_2D,
      .size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
      .format = WGPUTextureFormat_BGRA8Unorm,
      .mipLevelCount = 1,
      .sampleCount = 1,
  };

  WGPUTexture offrenderTexture = wgpuDeviceCreateTexture(render->m_device, &textureDesc);
  offrenderOutTextureView = wgpuTextureCreateView(offrenderTexture, nullptr);

  static std::vector<WGPUBindGroupEntry> bgEntriesPpfx(2);

  bgEntriesPpfx[0].binding = 0;
  bgEntriesPpfx[0].textureView = offrenderOutTextureView;
  bgEntriesPpfx[0].offset = 0;

  bgEntriesPpfx[1].binding = 1;
  bgEntriesPpfx[1].sampler = render->m_sampler;
  bgEntriesPpfx[1].offset = 0;

  WGPUBindGroupDescriptor bgDescPpfx = {.label = "bg_ppfx"};
  bgDescPpfx.layout = wgpuRenderPipelineGetBindGroupLayout(m_pipeline_ppfx, 0);
  bgDescPpfx.entryCount = (uint32_t)bgEntriesPpfx.size();
  bgDescPpfx.entries = bgEntriesPpfx.data();

  bg_bgPpfx = wgpuDeviceCreateBindGroup(render->m_device, &bgDescPpfx);
}

void Application::OnStart() {
  render = std::make_unique<Render>();

  WGPUInstance instance = render->CreateInstance();
  render->m_window = static_cast<GLFWwindow*>(GetNativeWindow());

#if __EMSCRIPTEN__
  render->m_surface = htmlGetCanvasSurface(instance, "canvas");
#else
  render->m_surface = glfwGetWGPUSurface(instance, render->m_window);
#endif

  if (render->m_surface == nullptr) {
    RN_ERROR("Failed to create a rendering surface. The surface returned is null.");
    exit(-1);
  }

  render->Init(render->m_window, instance);

  Rain::ResourceManager::Init(std::make_shared<WGPUDevice>(render->m_device));
  Rain::ResourceManager::LoadTexture("T_Default", RESOURCE_DIR "/wood.png");

  shaderManager = std::make_shared<ShaderManager>(render->m_device);

  shaderManager->LoadShader("SH_Default", RESOURCE_DIR "/shader_default.wgsl");
  shaderManager->LoadShader("SH_PPFX", RESOURCE_DIR "/shader_ppfx.wgsl");
  shaderManager->LoadShader("SH_Shadow", RESOURCE_DIR "/shadow.wgsl");
  shaderManager->LoadShader("SH_Debug", RESOURCE_DIR "/debug.wgsl");

  pipelineManager = std::make_unique<PipelineManager>(render->m_device, shaderManager);

  int screenWidth, screenHeight;
  glfwGetFramebufferSize((GLFWwindow*)render->m_window, &screenWidth, &screenHeight);

  // Shared
  // =======================================================
	static std::vector<WGPUVertexAttribute> vertexAttribsScene(5);

	vertexAttribsScene[0] = {};
	vertexAttribsScene[0].shaderLocation = 0;
	vertexAttribsScene[0].offset = 0;
	vertexAttribsScene[0].format = WGPUVertexFormat_Float32x3;

	vertexAttribsScene[1] = {};
	vertexAttribsScene[1].shaderLocation = 1;
	vertexAttribsScene[1].offset = 16;
	vertexAttribsScene[1].format = WGPUVertexFormat_Float32x3;

	vertexAttribsScene[2] = {};
	vertexAttribsScene[2].shaderLocation = 2;
	vertexAttribsScene[2].offset = 32;
	vertexAttribsScene[2].format = WGPUVertexFormat_Float32x2;

	vertexAttribsScene[3] = {};
	vertexAttribsScene[3].shaderLocation = 3;
	vertexAttribsScene[3].offset = 48;
	vertexAttribsScene[3].format = WGPUVertexFormat_Float32x3;

	vertexAttribsScene[4] = {};
	vertexAttribsScene[4].shaderLocation = 4;
	vertexAttribsScene[4].offset = 64;
	vertexAttribsScene[4].format = WGPUVertexFormat_Float32x3;


  WGPUVertexBufferLayout avertexLayoutDefault = {};
	avertexLayoutDefault.attributeCount = vertexAttribsScene.size();
  avertexLayoutDefault.attributes = vertexAttribsScene.data();
  avertexLayoutDefault.arrayStride = 80;
  avertexLayoutDefault.stepMode = WGPUVertexStepMode_Vertex;

	static std::vector<WGPUVertexAttribute> vertexAttribsQuad(3);

	vertexAttribsQuad[0] = {};
	vertexAttribsQuad[0].shaderLocation = 0;
	vertexAttribsQuad[0].offset = 0;
	vertexAttribsQuad[0].format = WGPUVertexFormat_Float32x3;

	vertexAttribsQuad[1] = {};
	vertexAttribsQuad[1].shaderLocation = 1;
	vertexAttribsQuad[1].offset = 16;
	vertexAttribsQuad[1].format = WGPUVertexFormat_Float32x3;

  WGPUVertexBufferLayout vertexLayoutQuad = {};
  vertexLayoutQuad.attributeCount = 2;
  vertexLayoutQuad.attributes = vertexAttribsQuad.data();
  vertexLayoutQuad.arrayStride = 32;
  vertexLayoutQuad.stepMode = WGPUVertexStepMode_Vertex;


  // Prep. Shadow Resources
  // =======================================================

  GroupLayout groupLayoutShadow = {
      {0, GroupLayoutVisibility::Both, GroupLayoutType::Default},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {1, GroupLayoutVisibility::Both, GroupLayoutType::Default}};

  WGPUTexture shadowDepthTexture = render->GetDepthBufferTexture(render->m_device, render->m_depthTextureFormat, SHADOW_WIDTH, SHADOW_HEIGHT, true);
  shadowDepthView = render->GetDepthBufferTextureView("T_Depth_Shadow", shadowDepthTexture, render->m_depthTextureFormat);

  pipelineShadow = pipelineManager->CreatePipeline("RP_Shadow",
                                                   "SH_Shadow",
                                                   avertexLayoutDefault,
                                                   groupLayoutShadow,
                                                   render->m_depthTextureFormat,
                                                   WGPUTextureFormat_Undefined,
																									 WGPUCullMode_Back,
                                                   render->m_surface,
                                                   render->m_adapter);

  shadowPos = glm::vec3(-282, 2346, -65);
  shadowRot = glm::vec3(-66, 28, 0);
  //shadowRot = glm::vec3(-85, 28, 0);

  shadowView = GetViewMatrix(shadowPos, shadowRot);
  shadowProjection = glm::ortho(-SHADOW_WIDTH / 2, SHADOW_WIDTH / 2, -SHADOW_HEIGHT / 2, SHADOW_HEIGHT / 2,
                                SHADOW_NEAR,
                                SHADOW_FAR);

  WGPUBindGroupLayout bglScene = wgpuRenderPipelineGetBindGroupLayout(pipelineShadow, 0);

  // Prep. Offscreen Render Resources
  // =======================================================

  GroupLayout groupLayoutDefault = {
      {0, GroupLayoutVisibility::Both, GroupLayoutType::Default},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {1, GroupLayoutVisibility::Both, GroupLayoutType::Default},
      {2, GroupLayoutVisibility::Both, GroupLayoutType::TextureDepth},
      {2, GroupLayoutVisibility::Both, GroupLayoutType::SamplerCompare},
      {2, GroupLayoutVisibility::Both, GroupLayoutType::Default}};

  pipelineDefault = pipelineManager->CreatePipeline("RP_Default",
                                                    "SH_Default",
                                                    avertexLayoutDefault,
                                                    groupLayoutDefault,
                                                    render->m_depthTextureFormat,
                                                    render->m_swapChainFormat,
																										WGPUCullMode_None,
                                                    render->m_surface,
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

  WGPUBufferDescriptor shadowUniformDesc = {};
  shadowUniformDesc.size = sizeof(ShadowUniform);
  shadowUniformDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
  shadowUniformDesc.mappedAtCreation = false;

  shadowUniformBuffer = wgpuDeviceCreateBuffer(render->m_device, &shadowUniformDesc);

  shadowUniform.lightProjection = shadowProjection;
  shadowUniform.lightView = shadowView;
  shadowUniform.lightPos = shadowPos;

  wgpuQueueWriteBuffer(render->m_queue, shadowUniformBuffer, 0, &shadowUniform, sizeof(ShadowUniform));

  WGPUBindGroupLayout bglShadow = wgpuRenderPipelineGetBindGroupLayout(pipelineDefault, 2);
  static std::vector<WGPUBindGroupEntry> bgEntriesShadowMap(3);

  bgEntriesShadowMap[0].binding = 0;
  bgEntriesShadowMap[0].offset = 0;
  bgEntriesShadowMap[0].textureView = shadowDepthView;

  bgEntriesShadowMap[1].binding = 1;
  bgEntriesShadowMap[1].offset = 0;
  bgEntriesShadowMap[1].sampler = shadowSampler;

  bgEntriesShadowMap[2].binding = 2;
  bgEntriesShadowMap[2].offset = 0;
  bgEntriesShadowMap[2].buffer = shadowUniformBuffer;
  bgEntriesShadowMap[2].size = sizeof(ShadowUniform);

  static WGPUBindGroupDescriptor bgDescShadowMap = {.label = "bg_shadowmap"};
  bgDescShadowMap.layout = bglShadow;
  bgDescShadowMap.entryCount = (uint32_t)bgEntriesShadowMap.size();
  bgDescShadowMap.entries = bgEntriesShadowMap.data();

  bgShadowMap = wgpuDeviceCreateBindGroup(render->m_device, &bgDescShadowMap);

  Player = std::make_shared<PlayerCamera>();

  defaultCameraUniform.projectionMatrix = glm::perspective(glm::radians(FOV), (float)render->m_swapChainDesc.width / render->m_swapChainDesc.height, PERSPECTIVE_NEAR, PERSPECTIVE_FAR);
  defaultCameraUniform.viewMatrix = Player->GetViewMatrix();

  // Prep. Debug Resources
  // =======================================================

  BufferLayout vertexLayoutDebug = {
      {ShaderDataType::Float3, "position"},
      {ShaderDataType::Float2, "uv"}};

  GroupLayout groupLayoutDebug = {
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::TextureDepth},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Default}};

  static DebugUniform debugUniform = {
      .near = SHADOW_NEAR,
      .far = SHADOW_FAR};

  WGPUBufferDescriptor debugUniformDesc = {};
  debugUniformDesc.size = sizeof(DebugUniform);
  debugUniformDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
  debugUniformDesc.mappedAtCreation = false;

  static WGPUBuffer debugUniformBuffer;
  debugUniformBuffer = wgpuDeviceCreateBuffer(render->m_device, &debugUniformDesc);
  wgpuQueueWriteBuffer(render->m_queue, debugUniformBuffer, 0, &debugUniform, sizeof(DebugUniform));

  static std::vector<WGPUBindGroupEntry> bgEntriesDebug(3);

  bgEntriesDebug[0].binding = 0;
  bgEntriesDebug[0].textureView = shadowDepthView;
  bgEntriesDebug[0].offset = 0;

  bgEntriesDebug[1].binding = 1;
  bgEntriesDebug[1].sampler = render->m_sampler;
  bgEntriesDebug[1].offset = 0;

  bgEntriesDebug[2].binding = 2;
  bgEntriesDebug[2].buffer = debugUniformBuffer;
  bgEntriesDebug[2].size = sizeof(DebugUniform);
  bgEntriesDebug[2].offset = 0;

  pipelineDebug = pipelineManager->CreatePipeline("RP_Debug",
                                                  "SH_Debug",
                                                  vertexLayoutQuad,
                                                  groupLayoutDebug,
                                                  WGPUTextureFormat_Undefined,
                                                  render->m_swapChainFormat,
																									WGPUCullMode_None,
                                                  render->m_surface,
                                                  render->m_adapter);

  static WGPUBindGroupDescriptor bgDescDebug = {.label = "bg_debug"};
  bgDescDebug.layout = wgpuRenderPipelineGetBindGroupLayout(pipelineDebug, 0);
  bgDescDebug.entryCount = (uint32_t)bgEntriesDebug.size();
  bgDescDebug.entries = bgEntriesDebug.data();

  bgDebug = wgpuDeviceCreateBindGroup(render->m_device, &bgDescDebug);

  WGPUTextureDescriptor textureDescDebug = {
      .usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
      .dimension = WGPUTextureDimension_2D,
      .size = {static_cast<uint32_t>(screenWidth), static_cast<uint32_t>(screenHeight), 1},
      .format = WGPUTextureFormat_BGRA8Unorm,
      .mipLevelCount = 1,
      .sampleCount = 1,
  };

  WGPUTexture debugOutTexture = wgpuDeviceCreateTexture(render->m_device, &textureDescDebug);
  debugOutTextureView = wgpuTextureCreateView(debugOutTexture, nullptr);

  // Prep. PPFX Resources
  // =======================================================

  BufferLayout vertexLayoutPpfx = {
      {ShaderDataType::Float3, "position"},
      {ShaderDataType::Float2, "uv"}};

  GroupLayout groupLayoutPpfx = {
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler}};

  m_pipeline_ppfx = pipelineManager->CreatePipeline("RP_PPFX",
                                                    "SH_PPFX",
                                                    vertexLayoutQuad,
                                                    groupLayoutPpfx,
                                                    render->m_depthTextureFormat,
                                                    render->m_swapChainFormat,
																										WGPUCullMode_None,
                                                    render->m_surface,
                                                    render->m_adapter);

  ppfxCreateBindings(screenWidth, screenHeight);

  WGPUBufferDescriptor vertexBufferDesc = {};
  vertexBufferDesc.size = sizeof(quadVertices);
  vertexBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
  vertexBufferDesc.mappedAtCreation = false;
  vertexBufferPpfx = wgpuDeviceCreateBuffer(render->m_device, &vertexBufferDesc);

  wgpuQueueWriteBuffer(render->m_queue, vertexBufferPpfx, 0, quadVertices, vertexBufferDesc.size);

  WGPUBufferDescriptor indexBufferDesc = {};
  indexBufferDesc.size = sizeof(quadIndices);
  indexBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
  indexBufferDesc.mappedAtCreation = false;
  indexBufferPpfx = wgpuDeviceCreateBuffer(render->m_device, &indexBufferDesc);
  wgpuQueueWriteBuffer(render->m_queue, indexBufferPpfx, 0, quadIndices, sizeof(quadIndices));

  // Prep. Camera
  // =======================================================

  WGPUBindGroupLayout bglCamera = wgpuRenderPipelineGetBindGroupLayout(pipelineDefault, 1);

  WGPUBufferDescriptor cameraUniformDesc = {};
  cameraUniformDesc.size = sizeof(CameraUniform);
  cameraUniformDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
  cameraUniformDesc.mappedAtCreation = false;

  defaultCameraUniformBuffer = wgpuDeviceCreateBuffer(render->m_device, &cameraUniformDesc);
  shadowCameraUniformBuffer = wgpuDeviceCreateBuffer(render->m_device, &cameraUniformDesc);

  std::vector<WGPUBindGroupEntry> bgEntriesCamera(1);

  bgEntriesCamera[0].binding = 0;
  bgEntriesCamera[0].buffer = defaultCameraUniformBuffer;
  bgEntriesCamera[0].offset = 0;
  bgEntriesCamera[0].size = sizeof(CameraUniform);

  static WGPUBindGroupDescriptor bgDescDefaultCamera = {};
  bgDescDefaultCamera.layout = bglCamera;
  bgDescDefaultCamera.entryCount = (uint32_t)bgEntriesCamera.size();
  bgDescDefaultCamera.entries = bgEntriesCamera.data();

  bgCameraDefault = wgpuDeviceCreateBindGroup(render->m_device, &bgDescDefaultCamera);

  bgEntriesCamera[0].buffer = shadowCameraUniformBuffer;

  static WGPUBindGroupDescriptor bgDescShadowCamera = {};
  bgDescShadowCamera.layout = bglCamera;
  bgDescShadowCamera.entryCount = (uint32_t)bgEntriesCamera.size();
  bgDescShadowCamera.entries = bgEntriesCamera.data();

  bgCameraShadow = wgpuDeviceCreateBindGroup(render->m_device, &bgDescShadowCamera);

  shadowCameraUniform.projectionMatrix = shadowProjection;
  shadowCameraUniform.viewMatrix = shadowView;

  wgpuQueueWriteBuffer(render->m_queue, defaultCameraUniformBuffer, 0, &defaultCameraUniform, sizeof(CameraUniform));
  wgpuQueueWriteBuffer(render->m_queue, shadowCameraUniformBuffer, 0, &shadowCameraUniform, sizeof(CameraUniform));

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

  sponza = new Model(RESOURCE_DIR "/sponza.obj", bglScene, render->m_device, render->m_queue, render->m_sampler);
  sponza->Transform.scale = glm::vec3(0.2f);
  sponza->UpdateUniforms(render->m_queue);

  Cursor::Setup(render->m_window);
  Keyboard::Setup(render->m_window);

  Cursor::CaptureMouse(true);
}

void Application::OnResize(int height, int width) {
  render->m_swapChainDesc.height = height;
  render->m_swapChainDesc.width = width;

  render->m_swapChain = render->buildSwapChain(render->m_swapChainDesc, render->m_device, render->m_surface);
  render->m_depthTexture = render->GetDepthBufferTexture(render->m_device, render->m_depthTextureFormat, render->m_swapChainDesc.width, render->m_swapChainDesc.height);
  render->m_depthTextureView = render->GetDepthBufferTextureView("T_DepthDefault", render->m_depthTexture, render->m_depthTextureFormat);

  ppfxCreateBindings(width, height);

  defaultProjection = glm::perspective(glm::radians(FOV), (float)render->m_swapChainDesc.width / render->m_swapChainDesc.height, PERSPECTIVE_NEAR, PERSPECTIVE_FAR);
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

  defaultCameraUniform.viewMatrix = Player->GetViewMatrix();
  wgpuQueueWriteBuffer(render->m_queue, defaultCameraUniformBuffer, 0, &defaultCameraUniform, sizeof(CameraUniform));
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

  shadowPassDesc.colorAttachmentCount = 0;
  shadowPassDesc.depthStencilAttachment = &shadowDepthAttachment;

  WGPURenderPassEncoder shadowPassEncoder = wgpuCommandEncoderBeginRenderPass(render->encoder, &shadowPassDesc);

  wgpuRenderPassEncoderSetBindGroup(shadowPassEncoder, 1, bgCameraShadow, 0, NULL);
  wgpuRenderPassEncoderSetViewport(shadowPassEncoder, 0, 0, SHADOW_WIDTH, SHADOW_HEIGHT, 0, 1);

  sponza->Draw(shadowPassEncoder, pipelineShadow);

  wgpuRenderPassEncoderEnd(shadowPassEncoder);

  // Debug Pass
  // =======================================================

  //  WGPURenderPassColorAttachment debugColorAttachment = {};
  //  debugColorAttachment.loadOp = WGPULoadOp_Clear;
  //  debugColorAttachment.storeOp = WGPUStoreOp_Store;
  //  debugColorAttachment.clearValue = {0.0, 0.0, 0.0, 1.0};
  //  debugColorAttachment.view = debugOutTextureView;
  // #if !__EMSCRIPTEN__
  //  debugColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
  // #endif
  //
  //  WGPURenderPassDescriptor debugPassDesc{.label = "debug_pass"};
  //  debugPassDesc.colorAttachmentCount = 1;
  //  debugPassDesc.timestampWrites = 0;
  //  debugPassDesc.timestampWrites = nullptr;
  //  debugPassDesc.colorAttachments = &debugColorAttachment;
  //
  //  WGPURenderPassEncoder debugPassEncoder = wgpuCommandEncoderBeginRenderPass(render->encoder, &debugPassDesc);
  //
  //  wgpuRenderPassEncoderSetPipeline(debugPassEncoder, m_pipeline_debug);
  //  wgpuRenderPassEncoderSetVertexBuffer(debugPassEncoder, 0, vertexBufferPpfx, 0, sizeof(quadVertices));
  //  wgpuRenderPassEncoderSetIndexBuffer(debugPassEncoder, indexBufferPpfx, WGPUIndexFormat_Uint32, 0, sizeof(quadIndices));
  //  wgpuRenderPassEncoderSetBindGroup(debugPassEncoder, 0, m_bind_group_debug, 0, nullptr);
  //
  //  wgpuRenderPassEncoderDrawIndexed(debugPassEncoder, 6, 1, 0, 0, 0);
  //
  //  wgpuRenderPassEncoderEnd(debugPassEncoder);

  // Lit Pass
  // =======================================================

  WGPURenderPassColorAttachment litColorAttachment{};
  litColorAttachment.loadOp = WGPULoadOp_Clear;
  litColorAttachment.storeOp = WGPUStoreOp_Store;
  litColorAttachment.clearValue = WGPUColor{0.52, 0.80, 0.92, 1};
  litColorAttachment.resolveTarget = nullptr;
  litColorAttachment.view = offrenderOutTextureView;
#if !__EMSCRIPTEN__
  litColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
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
  litPassDesc.colorAttachments = &litColorAttachment;
  litPassDesc.depthStencilAttachment = &litDepthAttachment;

  WGPURenderPassEncoder litPassEncoder = wgpuCommandEncoderBeginRenderPass(render->encoder, &litPassDesc);

  wgpuRenderPassEncoderSetBindGroup(litPassEncoder, 1, bgCameraDefault, 0, NULL);
  wgpuRenderPassEncoderSetBindGroup(litPassEncoder, 2, bgShadowMap, 0, NULL);

  sponza->Draw(litPassEncoder, pipelineDefault);

  wgpuRenderPassEncoderEnd(litPassEncoder);

  // PPFX Pass
  // =======================================================

  WGPURenderPassColorAttachment ppfxColorAttachment = {};
  ppfxColorAttachment.loadOp = WGPULoadOp_Clear;
  ppfxColorAttachment.storeOp = WGPUStoreOp_Store;
  ppfxColorAttachment.clearValue = {0.0, 0.0, 0.0, 1.0};
  ppfxColorAttachment.view = render->nextTexture;
#if !__EMSCRIPTEN__
  ppfxColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

  WGPURenderPassDescriptor ppfxPassDesc{.label = "ppfx_pass"};
  ppfxPassDesc.colorAttachmentCount = 1;
  ppfxPassDesc.timestampWrites = 0;
  ppfxPassDesc.timestampWrites = nullptr;
  ppfxPassDesc.colorAttachments = &ppfxColorAttachment;
  ppfxPassDesc.depthStencilAttachment = &litDepthAttachment;  // we use for ImGui

  WGPURenderPassEncoder ppfxPassEncoder = wgpuCommandEncoderBeginRenderPass(render->encoder, &ppfxPassDesc);

  wgpuRenderPassEncoderSetPipeline(ppfxPassEncoder, m_pipeline_ppfx);
  wgpuRenderPassEncoderSetVertexBuffer(ppfxPassEncoder, 0, vertexBufferPpfx, 0, sizeof(quadVertices));
  wgpuRenderPassEncoderSetIndexBuffer(ppfxPassEncoder, indexBufferPpfx, WGPUIndexFormat_Uint32, 0, sizeof(quadIndices));
  wgpuRenderPassEncoderSetBindGroup(ppfxPassEncoder, 0, bg_bgPpfx, 0, nullptr);

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

  defaultCameraUniform.viewMatrix = Player->GetViewMatrix();
  wgpuQueueWriteBuffer(render->m_queue, defaultCameraUniformBuffer, 0, &defaultCameraUniform, sizeof(CameraUniform));
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
  bool updateShadows = false;

  if (ImGui::InputFloat(_labelPrefix("Light Pos X").c_str(), &shadowPos.x, 1.0f)) {
    updateShadows = true;
  }
  if (ImGui::InputFloat(_labelPrefix("Light Pos Y: ").c_str(), &shadowPos.y, 1.0f)) {
    updateShadows = true;
  }
  if (ImGui::InputFloat(_labelPrefix("Light Pos Z").c_str(), &shadowPos.z, 1.0f)) {
    updateShadows = true;
  }

  ImGui::Spacing();

  if (ImGui::InputFloat(_labelPrefix("Shadow Rot X").c_str(), &shadowRot.x, 0.5f)) {
    updateShadows = true;
  }
  if (ImGui::InputFloat(_labelPrefix("Shadow Rot Y").c_str(), &shadowRot.y, 0.5f)) {
    updateShadows = true;
  }
  if (ImGui::InputFloat(_labelPrefix("Shadow Rot Z").c_str(), &shadowRot.z, 0.5f)) {
    updateShadows = true;
  }

  if (updateShadows) {
    shadowUniform.lightPos = shadowPos;

    shadowUniform.lightView = GetViewMatrix(shadowPos, shadowRot);
    shadowCameraUniform.viewMatrix = shadowUniform.lightView;

    wgpuQueueWriteBuffer(render->m_queue, shadowUniformBuffer, 0, &shadowUniform, sizeof(ShadowUniform));
    wgpuQueueWriteBuffer(render->m_queue, shadowCameraUniformBuffer, 0, &shadowUniform, sizeof(CameraUniform));
  }

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
