#pragma once
#include "core/Assert.h"
#include "scene/Components.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#define RN_DEBUG
#include <GLFW/glfw3.h>
#include <unistd.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <iostream>
#include "Application.h"

#include "Cam.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include "imgui.h"

#include "core/Log.h"
#include "io/cursor.h"
#include "io/keyboard.h"
#include "physics/Physics.h"
#include "render/GPUAllocator.h"
#include "render/PipelineManager.h"
#include "render/Render.h"
#include "render/ResourceManager.h"

// #include <PxPhysicsAPI.h>
// #include <pthread.h>

#define ENABLE_SHADOW_PASS 0
#define ENABLE_PPFX 0

#if __EMSCRIPTEN__
#include <emscripten.h>
#include "platform/web/web_window.h"
#endif

extern "C" WGPUSurface glfwGetWGPUSurface(WGPUInstance instance, GLFWwindow* window);

#define FOV 75.0f
#define PERSPECTIVE_NEAR 0.10f
#define PERSPECTIVE_FAR 2500.0f

float simElapsed = 0;

// struct CameraUniform {
//   glm::mat4x4 projectionMatrix;
//   glm::mat4x4 viewMatrix;
// };

std::unique_ptr<Render> render;
std::unique_ptr<Physics> physics;

std::shared_ptr<PlayerCamera> Player;

// GameObject* floorCube;
// std::vector<Ref<GameObject>> boxes;
// std::vector<Ref<GameObject>> projectiles;

std::unique_ptr<PipelineManager> pipelineManager;
std::shared_ptr<ShaderManager> shaderManager;

WGPURenderPipeline pipelineDefault = nullptr;
WGPURenderPipeline m_pipeline_ppfx = nullptr;

WGPUBindGroup bg_bgPpfx;

#if ENABLE_SHADOW_PASS
#define SHADOW_WIDTH 4096.0f
#define SHADOW_HEIGHT 4096.0f
#define SHADOW_NEAR 0.0f
#define SHADOW_FAR 1500.0f

struct ShadowUniform {
  glm::mat4x4 lightProjection;
  glm::mat4x4 lightView;
  glm::vec3 lightPos;
  int padding;
};

WGPURenderPipeline pipelineShadow = nullptr;
WGPUBindGroup bgShadowMap;
WGPUBindGroup bgCameraShadow;
ShadowUniform shadowUniform;
CameraUniform shadowCameraUniform;
WGPUBuffer shadowUniformBuffer;
WGPUBuffer shadowCameraUniformBuffer;

WGPUTextureView shadowDepthView;

glm::vec3 shadowPos = glm::vec3(0);
glm::vec3 shadowRot = glm::vec3(0);

glm::mat4 shadowProjection;
glm::mat4 shadowView;

struct DebugUniform {
  float near;
  float far;
};

WGPURenderPipeline pipelineDebug = nullptr;
WGPUBindGroup bgDebug;
WGPUTextureView debugOutTextureView;
#endif

//WGPUBindGroup bgCameraDefault;

WGPUBuffer vertexBufferPpfx;
WGPUBuffer indexBufferPpfx;

//CameraUniform defaultCameraUniform;
//WGPUBuffer defaultCameraUniformBuffer;

WGPUTextureView offrenderOutTextureView;

glm::mat4 defaultProjection;
glm::mat4 defaultView;

// PhysX Stuff
//=================================================================================
using namespace physx;

PxRigidStatic* gFloor = nullptr;
WGPUQuerySet m_timestampQueries = nullptr;
WGPUBuffer m_timestampResolveBuffer;
WGPUBuffer m_timestampMapBuffer;
std::unique_ptr<WGPUBufferMapCallback> m_timestampMapHandle = nullptr;

Application* Application::m_Instance;

void initBenchmark() {
  // Create timestamp queries
  WGPUQuerySetDescriptor querySetDesc;
  querySetDesc.type = WGPUQueryType_Timestamp;
  querySetDesc.count = 2;  // start and end
  querySetDesc.nextInChain = nullptr;
  m_timestampQueries = wgpuDeviceCreateQuerySet(render->m_device, &querySetDesc);

  // Create buffer to store timestamps
  WGPUBufferDescriptor bufferDesc;
  bufferDesc.label = "timestamp resolve buffer";
  bufferDesc.size = 2 * sizeof(uint64_t);
  bufferDesc.usage = WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc;
  bufferDesc.mappedAtCreation = false;
  bufferDesc.nextInChain = nullptr;
  m_timestampResolveBuffer = wgpuDeviceCreateBuffer(render->m_device, &bufferDesc);

  bufferDesc.label = "timestamp map buffer";
  bufferDesc.size = 2 * sizeof(uint64_t);
  bufferDesc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
  m_timestampMapBuffer = wgpuDeviceCreateBuffer(render->m_device, &bufferDesc);
}

void CallBack(WGPUBufferMapAsyncStatus status, void* userData) {
  if (status != WGPUBufferMapAsyncStatus_Success) {
    std::cerr << "Could not map buffer! status = " << status << std::endl;
  } else {
    uint64_t* timestampData = (uint64_t*)wgpuBufferGetConstMappedRange(m_timestampMapBuffer, 0, 2 * sizeof(uint64_t));

    Application* app = (Application*)userData;

    // Use timestampData
    uint64_t begin = timestampData[0];
    uint64_t end = timestampData[1];
    uint64_t nanoseconds = (end - begin);
    float milliseconds = (float)nanoseconds * 1e-6;
    // std::cout << "Render pass took " << milliseconds << "ms" << std::endl;
    app->m_perf.add_sample(milliseconds * 1e-3);

    wgpuBufferUnmap(m_timestampMapBuffer);
  }

  m_timestampMapHandle.reset();
}

void Application::fetchTimestamps() {
  // If we are already in the middle of a mapping operation,
  // no need to trigger a new one.
  if (m_timestampMapHandle) {
    return;
  }
  assert(m_timestampMapBuffer.getMapState() == BufferMapState::Unmapped);

  m_timestampMapHandle = std::make_unique<WGPUBufferMapCallback>(CallBack);

  wgpuBufferMapAsync(m_timestampMapBuffer, WGPUMapMode_Read, 0, 2 * sizeof(uint64_t), CallBack, this);
}

void createPhysXObjects() {
  PxMaterial* material = physics->gPhysics->createMaterial(0.5f, 2.5f, 0.0f);

  PxVec3 dimensions(50.0f, 1.0f, 50.0f);  // Scale of the cube
  PxShape* shape = physics->gPhysics->createShape(PxBoxGeometry(dimensions.x, dimensions.y, dimensions.z), *material);

  if (!shape) {
    throw std::runtime_error("Failed to create shape.");
  }

  PxRigidStatic* staticActor = physics->gPhysics->createRigidStatic(PxTransform(PxVec3(0.0f, -1.0f, 0.0f)));
  if (!staticActor) {
    throw std::runtime_error("Failed to create static actor.");
  }

  staticActor->attachShape(*shape);
  shape->release();

  physics->gScene->addActor(*staticActor);
}

void resolveTimestamps(WGPUCommandEncoder encoder) {
  // If we are already in the middle of a mapping operation,
  // no need to trigger a new one.
  if (m_timestampMapHandle) {
    return;
  }

  // Resolve the timestamp queries (write their result to the resolve buffer)
  wgpuCommandEncoderResolveQuerySet(encoder,
                                    m_timestampQueries,
                                    0, 2,  // get queries 0 to 0+2
                                    m_timestampResolveBuffer,
                                    0);

  // Copy to the map buffer
  wgpuCommandEncoderCopyBufferToBuffer(encoder,
                                       m_timestampResolveBuffer, 0,
                                       m_timestampMapBuffer, 0,
                                       2 * sizeof(uint64_t));
}

//=================================================================================

#if ENABLE_SHADOW_PASS
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
#endif

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

// static int sphereIdNext = 0;
// void shootSphere(const glm::vec3& startPos, const glm::vec3& velocity) {
//   std::string id = std::to_string(sphereIdNext++) + "_sphere";
//   Ref<GameObject> projectile = CreateRef<GameObject>(id, sphereModel);
//   projectile->Transform.scale = glm::vec3(0.5f);
//   projectile->Transform.position = startPos;
//
//   projectile->AddPhysicsSphere(velocity);
//   projectiles.push_back(projectile);
// }

Scene* scene;
void Application::OnStart() {
  m_Instance = this;
  render = std::make_unique<Render>();
  physics = std::make_unique<Physics>();
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


  Rain::ResourceManager::Init(std::make_shared<WGPUDevice>(render->m_device));
  Rain::ResourceManager::LoadTexture("T_Default", RESOURCE_DIR "/textures/placeholder.jpeg");

  //shaderManager = std::make_shared<ShaderManager>(render->m_device);

  //shaderManager->LoadShader("SH_Default", RESOURCE_DIR "/shaders/default.wgsl");
  //shaderManager->LoadShader("SH_DefaultBasic", RESOURCE_DIR "/shaders/default_basic.wgsl");
  //shaderManager->LoadShader("SH_DefaultBasicBatch", RESOURCE_DIR "/shaders/default_basic_batch.wgsl");
  //shaderManager->LoadShader("SH_PPFX", RESOURCE_DIR "/shaders/ppfx.wgsl");
#if ENABLE_SHADOW_PASS
  shaderManager->LoadShader("SH_DefaultBasicBatchDebug", RESOURCE_DIR "/shaders/default_debug.wgsl");
  shaderManager->LoadShader("SH_Shadow", RESOURCE_DIR "/shaders/shadow_map_batch.wgsl");
  shaderManager->LoadShader("SH_Debug", RESOURCE_DIR "/shaders/debug.wgsl");
#endif

  //pipelineManager = std::make_unique<PipelineManager>(render->m_device, shaderManager);

  int screenWidth, screenHeight;
  glfwGetFramebufferSize((GLFWwindow*)render->m_window, &screenWidth, &screenHeight);

  // Shared
  // =======================================================

//  static std::vector<WGPUVertexAttribute> vaDefault(4);
//
//  vaDefault[0] = {};
//  vaDefault[0].shaderLocation = 0;
//  vaDefault[0].offset = 0;
//  vaDefault[0].format = WGPUVertexFormat_Float32x3;
//
//  vaDefault[1] = {};
//  vaDefault[1].shaderLocation = 1;
//  vaDefault[1].offset = 16;
//  vaDefault[1].format = WGPUVertexFormat_Float32x3;
//
//  vaDefault[2] = {};
//  vaDefault[2].shaderLocation = 2;
//  vaDefault[2].offset = 32;
//  vaDefault[2].format = WGPUVertexFormat_Float32x2;
//
//  vaDefault[3] = {};
//  vaDefault[3].shaderLocation = 3;
//  vaDefault[3].offset = 48;
//  vaDefault[3].format = WGPUVertexFormat_Float32x3;
//
//  static std::vector<WGPUVertexAttribute> vaInstance(3);
//
//  vaInstance[0] = {};
//  vaInstance[0].shaderLocation = 4;
//  vaInstance[0].offset = 0;
//  vaInstance[0].format = WGPUVertexFormat_Float32x4;
//
//  vaInstance[1] = {};
//  vaInstance[1].shaderLocation = 5;
//  vaInstance[1].offset = 16;
//  vaInstance[1].format = WGPUVertexFormat_Float32x4;
//
//  vaInstance[2] = {};
//  vaInstance[2].shaderLocation = 6;
//  vaInstance[2].offset = 32;
//  vaInstance[2].format = WGPUVertexFormat_Float32x4;
//
//  WGPUVertexBufferLayout vblInstance = {};
//  vblInstance.attributeCount = vaInstance.size();
//  vblInstance.attributes = vaInstance.data();
//  vblInstance.arrayStride = 48;
//  vblInstance.stepMode = WGPUVertexStepMode_Instance;
//
//  WGPUVertexBufferLayout vblDefault = {};
//  vblDefault.attributeCount = vaDefault.size();
//  vblDefault.attributes = vaDefault.data();
//  vblDefault.arrayStride = 64;
//  vblDefault.stepMode = WGPUVertexStepMode_Vertex;
//
//  static std::vector<WGPUVertexAttribute> vertexAttribsQuad(3);
//
//  vertexAttribsQuad[0] = {};
//  vertexAttribsQuad[0].shaderLocation = 0;
//  vertexAttribsQuad[0].offset = 0;
//  vertexAttribsQuad[0].format = WGPUVertexFormat_Float32x3;
//
//  vertexAttribsQuad[1] = {};
//  vertexAttribsQuad[1].shaderLocation = 1;
//  vertexAttribsQuad[1].offset = 16;
//  vertexAttribsQuad[1].format = WGPUVertexFormat_Float32x3;
//
//  WGPUVertexBufferLayout vertexLayoutQuad = {};
//  vertexLayoutQuad.attributeCount = 2;
//  vertexLayoutQuad.attributes = vertexAttribsQuad.data();
//  vertexLayoutQuad.arrayStride = 32;
//  vertexLayoutQuad.stepMode = WGPUVertexStepMode_Vertex;
//
//  GroupLayout sceneGroup = {
//      {0, GroupLayoutVisibility::Vertex, GroupLayoutType::StorageReadOnly}};
//
//  GroupLayout cameraGroup = {
//      {0, GroupLayoutVisibility::Both, GroupLayoutType::Uniform}};
//
//  GroupLayout materialGroup = {
//      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
//      {1, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler},
//      {2, GroupLayoutVisibility::Both, GroupLayoutType::Uniform}};
//
//  GroupLayout shadowGroup = {
//      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::TextureDepth},
//      {1, GroupLayoutVisibility::Fragment, GroupLayoutType::SamplerCompare},
//      {2, GroupLayoutVisibility::Both, GroupLayoutType::Uniform}};
//
//  GroupLayout ppfxGroup = {
//      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
//      {1, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler}};
//
//  GroupLayout debugGroup = {
//      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::TextureDepth},
//      {1, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler},
//      {2, GroupLayoutVisibility::Fragment, GroupLayoutType::Uniform}};
//
//  static std::map<int, GroupLayout> layoutShadow;
//  layoutShadow.insert({0, sceneGroup});
//  layoutShadow.insert({1, cameraGroup});
//
//  static std::map<int, GroupLayout> layoutDefault;
//  //layoutDefault.insert({0, sceneGroup});
//  layoutDefault.insert({0, cameraGroup});
//  layoutDefault.insert({1, materialGroup});
//  //layoutDefault.insert({3, shadowGroup});
//
//  static std::map<int, GroupLayout> layoutPpfx;
//  layoutPpfx.insert({0, ppfxGroup});
//
//  static std::map<int, GroupLayout> layoutDebug;
//  layoutDebug.insert({0, debugGroup});
//
//  BufferLayout vertexLayoutPpfx = {
//      {ShaderDataType::Float3, "position"},
//      {ShaderDataType::Float2, "uv"}};
//
#if ENABLE_SHADOW_PASS
  // Prep. Shadow Resources
  // =======================================================

  WGPUTexture shadowDepthTexture = render->GetDepthBufferTexture(render->m_device, render->m_depthTextureFormat, SHADOW_WIDTH, SHADOW_HEIGHT, true);
  shadowDepthView = render->GetDepthBufferTextureView("T_Depth_Shadow", shadowDepthTexture, render->m_depthTextureFormat);

  pipelineShadow = pipelineManager->CreatePipeline("RP_Shadow",
                                                   "SH_Shadow",
                                                   avertexLayoutDefault,
                                                   layoutShadow,
                                                   render->m_depthTextureFormat,
                                                   WGPUTextureFormat_Undefined,
                                                   WGPUCullMode_Back,
                                                   render->m_surface,
                                                   render->m_adapter);

  // shadowPos = glm::vec3(-166, 179, 0);
  // shadowRot = glm::vec3(-48, 0, 0);

  shadowPos = glm::vec3(212, 852, 71);
  shadowRot = glm::vec3(-107, 35, 0);

  shadowView = GetViewMatrix(shadowPos, shadowRot);
  const float shadowFrustum = 200;
  shadowProjection = glm::ortho(-shadowFrustum, shadowFrustum, -shadowFrustum, shadowFrustum,
                                SHADOW_NEAR,
                                SHADOW_FAR);
#endif

//	static std::vector<WGPUVertexBufferLayout> vls;
//	vls.push_back(vblDefault);
//	vls.push_back(vblInstance);
//
//  pipelineDefault = pipelineManager->CreatePipeline("RP_Default",
//                                                    "SH_DefaultBasicBatch",
//                                                    vls,
//                                                    layoutDefault,
//                                                    render->m_depthTextureFormat,
//                                                    render->m_swapChainFormat,
//                                                    WGPUCullMode_Back,
//                                                    render->m_surface,
//                                                    render->m_adapter);
//
#if ENABLE_SHADOW_PASS
  // Prep. Offscreen Render Resources
  // =======================================================

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

  WGPUBindGroupLayout bglShadow = wgpuRenderPipelineGetBindGroupLayout(pipelineDefault, 3);
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

#endif

  Player = std::make_shared<PlayerCamera>();
  Player->Position.y = 0;
  Player->Position.z = 0;

  //defaultCameraUniform.projectionMatrix = glm::perspective(glm::radians(FOV), (float)render->m_swapChainDesc.width / render->m_swapChainDesc.height, PERSPECTIVE_NEAR, PERSPECTIVE_FAR);
  //defaultCameraUniform.viewMatrix = Player->GetViewMatrix();

#if ENABLE_SHADOW_PASS
  // Prep. Debug Resources
  // =======================================================

  BufferLayout vertexLayoutDebug = {
      {ShaderDataType::Float3, "position"},
      {ShaderDataType::Float2, "uv"}};

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
                                                  layoutDebug,
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
#endif

#if ENABLE_PPFX
  // Prep. PPFX Resources
  // =======================================================

  m_pipeline_ppfx = pipelineManager->CreatePipeline("RP_PPFX",
                                                    "SH_PPFX",
                                                    vertexLayoutQuad,
                                                    layoutPpfx,
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
#endif

  // Prep. Camera
  // =======================================================

  //WGPUBindGroupLayout bglCamera = wgpuRenderPipelineGetBindGroupLayout(pipelineDefault, 0);

  //WGPUBufferDescriptor cameraUniformDesc = {};
  //cameraUniformDesc.size = sizeof(CameraUniform);
  //cameraUniformDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
  //cameraUniformDesc.mappedAtCreation = false;

  //defaultCameraUniformBuffer = wgpuDeviceCreateBuffer(render->m_device, &cameraUniformDesc);

  //std::vector<WGPUBindGroupEntry> bgEntriesCamera(1);

  //bgEntriesCamera[0].binding = 0;
  //bgEntriesCamera[0].buffer = defaultCameraUniformBuffer;
  //bgEntriesCamera[0].offset = 0;
  //bgEntriesCamera[0].size = sizeof(CameraUniform);

  //static WGPUBindGroupDescriptor bgDescDefaultCamera = {};
  //bgDescDefaultCamera.label = "bg_cam";
  //bgDescDefaultCamera.layout = bglCamera;
  //bgDescDefaultCamera.entryCount = (uint32_t)bgEntriesCamera.size();
  //bgDescDefaultCamera.entries = bgEntriesCamera.data();

  //bgCameraDefault = wgpuDeviceCreateBindGroup(render->m_device, &bgDescDefaultCamera);

  //wgpuQueueWriteBuffer(render->m_queue, defaultCameraUniformBuffer, 0, &defaultCameraUniform, sizeof(CameraUniform));

#if ENABLE_SHADOW_PASS
  shadowCameraUniformBuffer = wgpuDeviceCreateBuffer(render->m_device, &cameraUniformDesc);
  bgEntriesCamera[0].buffer = shadowCameraUniformBuffer;

  static WGPUBindGroupDescriptor bgDescShadowCamera = {};
  bgDescShadowCamera.label = "bg_shadow_cam";
  bgDescShadowCamera.layout = bglCamera;
  bgDescShadowCamera.entryCount = (uint32_t)bgEntriesCamera.size();
  bgDescShadowCamera.entries = bgEntriesCamera.data();

  bgCameraShadow = wgpuDeviceCreateBindGroup(render->m_device, &bgDescShadowCamera);

  shadowCameraUniform.projectionMatrix = shadowProjection;
  shadowCameraUniform.viewMatrix = shadowView;

  wgpuQueueWriteBuffer(render->m_queue, shadowCameraUniformBuffer, 0, &shadowCameraUniform, sizeof(CameraUniform));
#endif

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

  //scene->Init();
  // Ref<MeshSource> boxModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/models/box-test.gltf");
  // Ref<MeshSource> boxModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/models/box-test2.gltf");
  Ref<MeshSource> boxModel = Rain::ResourceManager::LoadMeshSource(RESOURCE_DIR "/models/SponzaExp5.gltf");

  Entity entityBox = scene->CreateEntity("Box");
  entityBox.GetComponent<TransformComponent>()->Translation = glm::vec3(0, 10, 0);
  scene->BuildMeshEntityHierarchy(entityBox, boxModel);

  // Entity entityBox2 = scene->CreateEntity("Box2");
  // scene->BuildMeshEntityHierarchy(entityBox2, boxModel);

  Cursor::Setup(render->m_window);
  Keyboard::Setup(render->m_window);
  // initBenchmark();
  // sleep(1);

  Cursor::CaptureMouse(true);
}

void Application::OnResize(int height, int width) {
  render->m_swapChainDesc.height = height;
  render->m_swapChainDesc.width = width;

  render->m_swapChain = render->BuildSwapChain(render->m_swapChainDesc, render->m_device, render->m_surface);
  render->m_depthTexture = render->GetDepthBufferTexture(render->m_device, render->m_depthTextureFormat, render->m_swapChainDesc.width, render->m_swapChainDesc.height);
  render->m_depthTextureView = render->GetDepthBufferTextureView("T_DepthDefault", render->m_depthTexture, render->m_depthTextureFormat);

  ppfxCreateBindings(width, height);

  defaultProjection = glm::perspective(glm::radians(FOV), (float)render->m_swapChainDesc.width / render->m_swapChainDesc.height, PERSPECTIVE_NEAR, PERSPECTIVE_FAR);
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

  //defaultCameraUniform.viewMatrix = Player->GetViewMatrix();
  //wgpuQueueWriteBuffer(render->m_queue, defaultCameraUniformBuffer, 0, &defaultCameraUniform, sizeof(CameraUniform));
}

void Application::OnUpdate() {
  // std::cout << "Update start" << std::endl;
  glfwPollEvents();
  MoveControls();

	scene->m_SceneCamera = Player.get();
  scene->OnUpdate();
  scene->OnRender(m_Renderer);

  return;
  auto start = std::chrono::high_resolution_clock::now();
  // physics->gScene->simulate(1.0f / 60.0f);
  // physics->gScene->fetchResults(true);
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration = end - start;

  // Output the duration
  simElapsed = duration.count();

  WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(render->m_swapChain);
  RN_CORE_ASSERT(nextTexture, "Failed to acquire next swap texture.");

  scene->OnUpdate();

  static WGPURenderPassTimestampWrites writes;
  writes.querySet = m_timestampQueries;
  writes.beginningOfPassWriteIndex = 0;
  writes.endOfPassWriteIndex = 1;

  WGPUCommandEncoderDescriptor commandEncoderDesc = {.label = "Command Encoder"};
  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(render->m_device, &commandEncoderDesc);

#if ENABLE_SHADOW_PASS

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
  shadowPassDesc.timestampWrites = nullptr;
  shadowPassDesc.colorAttachments = nullptr;

  shadowPassDesc.colorAttachmentCount = 0;
  shadowPassDesc.depthStencilAttachment = &shadowDepthAttachment;

  WGPURenderPassEncoder shadowPassEncoder = wgpuCommandEncoderBeginRenderPass(render->encoder, &shadowPassDesc);

  wgpuRenderPassEncoderSetBindGroup(shadowPassEncoder, 1, bgCameraShadow, 0, NULL);
  wgpuRenderPassEncoderSetViewport(shadowPassEncoder, 0, 0, SHADOW_WIDTH, SHADOW_HEIGHT, 0, 1);

  wgpuRenderPassEncoderEnd(shadowPassEncoder);

  // Debug Pass
  // =======================================================

  WGPURenderPassColorAttachment debugColorAttachment = {};
  debugColorAttachment.loadOp = WGPULoadOp_Clear;
  debugColorAttachment.storeOp = WGPUStoreOp_Store;
  debugColorAttachment.clearValue = {0.0, 0.0, 0.0, 1.0};
  debugColorAttachment.view = debugOutTextureView;
#if !__EMSCRIPTEN__
  // debugColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

  WGPURenderPassDescriptor debugPassDesc{.label = "debug_pass"};
  debugPassDesc.colorAttachmentCount = 1;
  debugPassDesc.timestampWrites = 0;
  debugPassDesc.timestampWrites = nullptr;
  debugPassDesc.colorAttachments = &debugColorAttachment;

  WGPURenderPassEncoder debugPassEncoder = wgpuCommandEncoderBeginRenderPass(render->encoder, &debugPassDesc);

  wgpuRenderPassEncoderSetPipeline(debugPassEncoder, pipelineDebug);
  wgpuRenderPassEncoderSetVertexBuffer(debugPassEncoder, 0, vertexBufferPpfx, 0, sizeof(quadVertices));
  wgpuRenderPassEncoderSetIndexBuffer(debugPassEncoder, indexBufferPpfx, WGPUIndexFormat_Uint32, 0, sizeof(quadIndices));
  wgpuRenderPassEncoderSetBindGroup(debugPassEncoder, 0, bgDebug, 0, nullptr);

  wgpuRenderPassEncoderDrawIndexed(debugPassEncoder, 6, 1, 0, 0, 0);

  wgpuRenderPassEncoderEnd(debugPassEncoder);
#endif

  // Lit Pass
  // =======================================================

  WGPURenderPassColorAttachment litColorAttachment{};
  litColorAttachment.loadOp = WGPULoadOp_Clear;
  litColorAttachment.storeOp = WGPUStoreOp_Store;
  litColorAttachment.clearValue = WGPUColor{0.52, 0.80, 0.92, 1};
  litColorAttachment.resolveTarget = nullptr;
#if ENABLE_PPFX
  litColorAttachment.view = offrenderOutTextureView;
#else
  litColorAttachment.view = nextTexture;
#endif
#if !__EMSCRIPTEN__
  // litColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
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

#if __EMSCRIPTEN__
  litPassDesc.timestampWrites = nullptr;
#else
  // litPassDesc.timestampWrites = &writes;
#endif

  litPassDesc.colorAttachments = &litColorAttachment;
  litPassDesc.depthStencilAttachment = &litDepthAttachment;

  WGPURenderPassEncoder litPassEncoder = wgpuCommandEncoderBeginRenderPass(encoder, &litPassDesc);

  //wgpuRenderPassEncoderSetBindGroup(litPassEncoder, 0, bgCameraDefault, 0, NULL);
#if ENABLE_SHADOW_PASS
  wgpuRenderPassEncoderSetBindGroup(litPassEncoder, 3, bgShadowMap, 0, NULL);
#endif

  scene->OnRender(m_Renderer);
  wgpuRenderPassEncoderEnd(litPassEncoder);

#if ENABLE_PPFX
  // PPFX Pass
  // =======================================================

  WGPURenderPassColorAttachment ppfxColorAttachment = {};
  ppfxColorAttachment.loadOp = WGPULoadOp_Clear;
  ppfxColorAttachment.storeOp = WGPUStoreOp_Store;
  ppfxColorAttachment.clearValue = {0.0, 0.0, 0.0, 1.0};
  ppfxColorAttachment.view = nextTexture;
#if !__EMSCRIPTEN__
  // ppfxColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
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
#endif

  // Submit
  // =======================================================

  // wgpuCommandEncoderWriteTimestamp(render->encoder, querySet, 1);

  // resolveTimestamps(render->encoder);
  wgpuTextureViewRelease(nextTexture);

  WGPUCommandBufferDescriptor cmdBufferDescriptor = {.label = "Command Buffer"};
  WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);

  wgpuQueueSubmit(render->m_queue, 1, &commandBuffer);

#if !__EMSCRIPTEN__
  // fetchTimestamps();
#endif

#ifndef __EMSCRIPTEN__
  wgpuSwapChainPresent(render->m_swapChain);
  wgpuDeviceTick(render->m_device);
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
  static glm::vec2 prevCursorPos = Cursor::GetCursorPosition();

  if (!Cursor::IsMouseCaptured()) {
    return;
  }

  glm::vec2 cursorPos = Cursor::GetCursorPosition();

  Player->ProcessMouseMovement(cursorPos.x - prevCursorPos.x, cursorPos.y - prevCursorPos.y);

  prevCursorPos = cursorPos;

  //defaultCameraUniform.viewMatrix = Player->GetViewMatrix();
  //wgpuQueueWriteBuffer(render->m_queue, defaultCameraUniformBuffer, 0, &defaultCameraUniform, sizeof(CameraUniform));
}

void Application::OnKeyPressed(KeyCode key, KeyAction action) {
  if (key == Rain::Key::Escape && action == Rain::Key::RN_KEY_RELEASE) {
    Cursor::CaptureMouse(false);
  }

  if (key == Rain::Key::F && action == Rain::Key::RN_KEY_RELEASE) {
    glm::vec3 shootDirection = Player->Front;
    glm::vec3 shootPosition = Player->Position + shootDirection;
    glm::vec3 shootVelocity = shootDirection * 50.0f;
    // shootSphere(shootPosition, shootVelocity);
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

  physx::PxU32 version = PX_PHYSICS_VERSION;
  physx::PxU32 major = (version >> 24) & 0xFF;
  physx::PxU32 minor = (version >> 16) & 0xFF;
  physx::PxU32 bugfix = (version >> 8) & 0xFF;

  ImGui::Text("PhysX Version: %d.%d.%d", major, minor, bugfix);

  ImGui::Spacing();

#if ENABLE_SHADOW_PASS
  bool updateShadows = false;

  ImGui::Text("Light Settings");
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
  ImGui::Image((ImTextureID)debugOutTextureView, ImVec2(500, 500));

  if (updateShadows) {
    shadowUniform.lightPos = shadowPos;

    shadowUniform.lightView = GetViewMatrix(shadowPos, shadowRot);
    shadowCameraUniform.viewMatrix = shadowUniform.lightView;

    wgpuQueueWriteBuffer(render->m_queue, shadowUniformBuffer, 0, &shadowUniform, sizeof(ShadowUniform));
    wgpuQueueWriteBuffer(render->m_queue, shadowCameraUniformBuffer, 0, &shadowUniform, sizeof(CameraUniform));
  }
#endif

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
  ImGui::Text("Physics simulation duration %.3f ms", simElapsed);
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
