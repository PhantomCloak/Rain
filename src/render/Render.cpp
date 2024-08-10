#include "Render.h"
#include "Mesh.h"
#include "ResourceManager.h"
#include "core/Assert.h"

#if __EMSCRIPTEN__
#include <emscripten.h>
#include "platform/web/web_window.h"
#else
#include <dawn/dawn_proc.h>
#endif

Render* Render::Instance = nullptr;

struct WGPURendererData {
  Ref<GPUBuffer> QuadVertexBuffer;
  Ref<GPUBuffer> QuadIndexBuffer;
};

struct ShaderDependencies {
  std::vector<Ref<RenderPipeline>> Pipelines;
  std::vector<Ref<Material>> Materials;
};

static ShaderDependencies s_ShaderDependencies;

static WGPURendererData* s_Data = nullptr;

extern "C" WGPUSurface glfwGetWGPUSurface(WGPUInstance instance, GLFWwindow* window);

WGPUInstance Render::CreateGPUInstance() {
  WGPUInstanceDescriptor instanceDesc = {};
	instanceDesc.nextInChain = nullptr;
  static WGPUInstance instance;

//#if !__EMSCRIPTEN__
//  static std::vector<const char*> enabledToggles = {
//      "allow_unsafe_apis",
//  };
//
//  WGPUDawnTogglesDescriptor dawnToggleDesc;
//  dawnToggleDesc.chain.next = nullptr;
//  dawnToggleDesc.chain.sType = WGPUSType_DawnTogglesDescriptor;
//
//  dawnToggleDesc.enabledToggles = enabledToggles.data();
//  dawnToggleDesc.enabledToggleCount = 1;
//  dawnToggleDesc.disabledToggleCount = 0;
//
//  instanceDesc.nextInChain = &dawnToggleDesc.chain;
//
//  instanceDesc.features.timedWaitAnyEnable = 0;
//  instanceDesc.features.timedWaitAnyMaxCount = 64;
//#endif

  instance = wgpuCreateInstance(&instanceDesc);

  RN_CORE_ASSERT(instance, "An error occured while acquiring the WebGPU instance.");

  return instance;
}

bool Render::Init(void* window) {
  Instance = this;

  auto instance = CreateGPUInstance();
  m_window = static_cast<GLFWwindow*>(window);

#if __EMSCRIPTEN__
  m_surface = htmlGetCanvasSurface(instance, "canvas");
#else
  m_surface = glfwGetWGPUSurface(instance, m_window);
#endif

  static WGPURequestAdapterOptions adapterOpts{};
  adapterOpts.compatibleSurface = m_surface;
  m_adapter = RequestAdapter(instance, &adapterOpts);

  WGPURequiredLimits requiredLimits = GetRequiredLimits(m_adapter);
  static WGPUDeviceDescriptor deviceDesc;

  deviceDesc.label = "My Device";
#if __EMSCRIPTEN__
  deviceDesc.requiredFeaturesCount = 0;
#else
  // deviceDesc.requiredFeaturesCount = 0;
  static WGPUFeatureName sucker[] = {WGPUFeatureName_TimestampQuery};
  deviceDesc.requiredFeatures = &sucker[0];
  deviceDesc.requiredFeatureCount = 1;
#endif
  deviceDesc.requiredLimits = &requiredLimits;
  deviceDesc.defaultQueue.label = "The default queue";

  m_device = RequestDevice(m_adapter, &deviceDesc);

  m_queue = wgpuDeviceGetQueue(m_device);

  RN_CORE_ASSERT(m_queue, "An error occured while acquiring the queue. this might indicate unsupported browser/device.");

  m_RenderContext = CreateRef<RenderContext>(m_adapter, m_surface, m_device, m_queue);

  int width, height;
  glfwGetFramebufferSize((GLFWwindow*)window, &width, &height);

  WGPUTextureFormat swapChainFormat;
#if __EMSCRIPTEN__
  swapChainFormat = wgpuSurfaceGetPreferredFormat(m_surface, m_adapter);
#else
  swapChainFormat = WGPUTextureFormat_BGRA8Unorm;
#endif

  m_swapChainFormat = swapChainFormat;
  m_swapChainDesc = {};

  m_swapChainDesc.width = (uint32_t)width;
  m_swapChainDesc.height = (uint32_t)height;
  m_swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
  m_swapChainDesc.format = swapChainFormat;
  m_swapChainDesc.presentMode = WGPUPresentMode_Fifo;

  if (m_swapChain != NULL) {
    wgpuSwapChainRelease(m_swapChain);
  }

  m_swapChain = BuildSwapChain(m_swapChainDesc, m_device, m_surface);

  wgpuDeviceSetUncapturedErrorCallback(
      m_device, [](WGPUErrorType errorType, const char* message, void* userdata) {
        fprintf(stderr, "Dawn error: %s\n", message);
        exit(0);
      },
      nullptr);

  RendererPostInit();
  return true;
}

void Render::RendererPostInit() {
  s_Data = new WGPURendererData();

  float x = -1;
  float y = -1;
  float width = 2, height = 2;
  struct QuadVertex {
    glm::vec3 Position;
    float _pad0 = 0;
    glm::vec2 TexCoord;
    float _pad1[2] = {0, 0};
  };

  QuadVertex* data = new QuadVertex[4];

  data[0].Position = glm::vec3(x, y, 0.0f);  // Bottom-left
  data[0].TexCoord = glm::vec2(0, 1);

  data[1].Position = glm::vec3(x + width, y, 0.0f);  // Bottom-right
  data[1].TexCoord = glm::vec2(1, 1);

  data[2].Position = glm::vec3(x + width, y + height, 0.0f);  // Top-right
  data[2].TexCoord = glm::vec2(1, 0);

  data[3].Position = glm::vec3(x, y + height, 0.0f);  // Top-left
  data[3].TexCoord = glm::vec2(0, 0);

  // TODO: Consider static buffers, immediately mapped buffers etc.
  s_Data->QuadVertexBuffer = GPUAllocator::GAlloc(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst, sizeof(QuadVertex) * 4);
  s_Data->QuadVertexBuffer->SetData(data, sizeof(QuadVertex) * 4);

  static uint32_t indices[6] = {
      0,
      1,
      2,
      2,
      3,
      0,
  };

  s_Data->QuadIndexBuffer = GPUAllocator::GAlloc(WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst, sizeof(uint32_t) * 6);
  s_Data->QuadIndexBuffer->SetData(indices, sizeof(uint32_t) * 6);
}

WGPUAdapter Render::RequestAdapter(WGPUInstance instance,
                                   WGPURequestAdapterOptions const* options) {
  struct UserData {
    WGPUAdapter adapter = nullptr;
    bool requestEnded = false;
  };
  UserData userData;

  auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status,
                                  WGPUAdapter adapter, char const* message,
                                  void* pUserData) {
    UserData& userData = *reinterpret_cast<UserData*>(pUserData);

    RN_CORE_ASSERT(status == WGPURequestAdapterStatus_Success, "An error occured while acquiring WebGPU adapter");

    userData.adapter = adapter;
    userData.requestEnded = true;
  };

  wgpuInstanceRequestAdapter(instance /* equivalent of navigator.gpu */,
                             options, onAdapterRequestEnded, (void*)&userData);

#if __EMSCRIPTEN__
  while (!userData.requestEnded) {
    emscripten_sleep(100);
  }
#endif

  assert(userData.requestEnded);

  return userData.adapter;
}

WGPUDevice Render::RequestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor) {
  struct UserData {
    WGPUDevice device = nullptr;
    bool requestEnded = false;
  };
  UserData userData;

  auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status,
                                 WGPUDevice device, char const* message,
                                 void* pUserData) {
    UserData& userData = *reinterpret_cast<UserData*>(pUserData);

    RN_CORE_ASSERT(status == WGPURequestDeviceStatus_Success, "An error occured while acquiring WebGPU adapter");

    userData.device = device;
    userData.requestEnded = true;
  };

  wgpuAdapterRequestDevice(adapter, descriptor, onDeviceRequestEnded,
                           (void*)&userData);

  // TODO: temporary hack for some browsers, it should be investigated more
#if __EMSCRIPTEN__
  while (!userData.requestEnded) {
    emscripten_sleep(100);
  }
#endif

  assert(userData.requestEnded);

  return userData.device;
}

WGPURequiredLimits Render::GetRequiredLimits(WGPUAdapter adapter) {
  static WGPUSupportedLimits supportedLimits = {};

#ifdef __EMSCRIPTEN__
  // Error in Chrome handling
  supportedLimits.limits.minStorageBufferOffsetAlignment = 256;
  supportedLimits.limits.minUniformBufferOffsetAlignment = 256;
#else
  wgpuAdapterGetLimits(adapter, &supportedLimits);
#endif

  static WGPURequiredLimits requiredLimits = {};
  requiredLimits.limits.maxVertexAttributes = supportedLimits.limits.maxVertexAttributes;
  requiredLimits.limits.maxVertexBuffers = supportedLimits.limits.maxVertexBuffers;
  requiredLimits.limits.maxBufferSize = 150000 * sizeof(VertexAttribute);
  requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttribute);
  requiredLimits.limits.minStorageBufferOffsetAlignment =
      supportedLimits.limits.minStorageBufferOffsetAlignment;
  requiredLimits.limits.minUniformBufferOffsetAlignment =
      supportedLimits.limits.minUniformBufferOffsetAlignment;
  requiredLimits.limits.maxInterStageShaderComponents = supportedLimits.limits.maxInterStageShaderComponents;
  requiredLimits.limits.maxBindGroups = supportedLimits.limits.maxBindGroups;
  requiredLimits.limits.maxUniformBuffersPerShaderStage = supportedLimits.limits.maxUniformBuffersPerShaderStage;
  requiredLimits.limits.maxUniformBufferBindingSize = 64 * 4 * sizeof(float);
  requiredLimits.limits.maxTextureDimension1D = 4096;
  requiredLimits.limits.maxTextureDimension2D = 4096;
  requiredLimits.limits.maxDynamicUniformBuffersPerPipelineLayout = supportedLimits.limits.maxDynamicUniformBuffersPerPipelineLayout;
  requiredLimits.limits.maxTextureArrayLayers = supportedLimits.limits.maxTextureArrayLayers;
  requiredLimits.limits.maxSampledTexturesPerShaderStage = supportedLimits.limits.maxSampledTexturesPerShaderStage;
  requiredLimits.limits.maxSamplersPerShaderStage = supportedLimits.limits.maxSamplersPerShaderStage;

  return requiredLimits;
}

WGPUSwapChain Render::BuildSwapChain(WGPUSwapChainDescriptor descriptor, WGPUDevice device, WGPUSurface surface) {
  if (m_swapChain != NULL) {
    wgpuSwapChainRelease(m_swapChain);
  }

  return wgpuDeviceCreateSwapChain(m_device, surface, &m_swapChainDesc);
}

Ref<Texture> Render::GetWhiteTexture() {
  static auto whiteTexture = Rain::ResourceManager::GetTexture("T_Default");
  RN_ASSERT(whiteTexture->View != 0, "Material: Default texture couldn't found.");
  return whiteTexture;
}

Ref<Sampler> Render::GetDefaultSampler() {
  static auto sampler = Sampler::Create(Sampler::GetDefaultProps("S_DefaultSampler"));
  RN_ASSERT(sampler != 0, "Material: Default sampler couldn't found.");
  return sampler;
}

WGPURenderPassEncoder Render::BeginRenderPass(Ref<RenderPass> pass, WGPUCommandEncoder& encoder) {
  auto& pipe = pass->GetProps().Pipeline;

  WGPURenderPassDescriptor passDesc{.label = pipe->GetName().c_str()};

  if (!pipe->HasColorAttachment()) {
    WGPURenderPassColorAttachment colorAttachment{};
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    colorAttachment.clearValue = WGPUColor{0, 0, 0, 1};
    colorAttachment.resolveTarget = nullptr;
    //colorAttachment.view = pipe->GetColorAttachment()->View;
    colorAttachment.view = Instance->GetCurrentSwapChainTexture()->View;

    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAttachment;
  } 

  if (pipe->HasDepthAttachment()) {
    WGPURenderPassDepthStencilAttachment depthAttachment;
    depthAttachment.view = pipe->GetDepthAttachment()->View;
    depthAttachment.depthClearValue = 1.0f;
    depthAttachment.depthLoadOp = WGPULoadOp_Clear;
    depthAttachment.depthStoreOp = WGPUStoreOp_Store;
    depthAttachment.depthReadOnly = false;
    depthAttachment.stencilClearValue = 0;
    depthAttachment.stencilLoadOp = WGPULoadOp_Undefined;
    depthAttachment.stencilStoreOp = WGPUStoreOp_Undefined;
    depthAttachment.stencilReadOnly = true;

    passDesc.depthStencilAttachment = &depthAttachment;
  } else {
    passDesc.depthStencilAttachment = nullptr;
  }

  auto renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
  auto bindings = pass->GetBindManager();

  for (auto& [index, bindGroup] : bindings->GetBindGroups()) {
    wgpuRenderPassEncoderSetBindGroup(renderPass, index, bindGroup, 0, 0);
  }

  return renderPass;
}

void Render::EndRenderPass(Ref<RenderPass> pass, WGPURenderPassEncoder& encoder) {
  wgpuRenderPassEncoderEnd(encoder);
}

void Render::RenderMesh(WGPURenderPassEncoder& renderCommandBuffer,
                        WGPURenderPipeline pipeline,
                        Ref<MeshSource> mesh,
                        uint32_t submeshIndex,
                        Ref<MaterialTable> materialTable,
                        Ref<GPUBuffer> transformBuffer,
                        uint32_t transformOffset,
                        uint32_t instanceCount) {
  wgpuRenderPassEncoderSetPipeline(renderCommandBuffer, pipeline);

  wgpuRenderPassEncoderSetVertexBuffer(renderCommandBuffer,
                                       0,
                                       mesh->GetVertexBuffer()->Buffer,
                                       0,
                                       mesh->GetVertexBuffer()->Size);

  wgpuRenderPassEncoderSetIndexBuffer(renderCommandBuffer,
                                      mesh->GetIndexBuffer()->Buffer,
                                      WGPUIndexFormat_Uint32,
                                      0,
                                      mesh->GetIndexBuffer()->Size);

  wgpuRenderPassEncoderSetVertexBuffer(renderCommandBuffer,
                                       1,
                                       transformBuffer->Buffer,
                                       transformOffset,
                                       transformBuffer->Size - transformOffset);

  const auto& subMesh = mesh->m_SubMeshes[submeshIndex];
  auto material = materialTable->HasMaterial(subMesh.MaterialIndex) ? materialTable->GetMaterial(subMesh.MaterialIndex) : mesh->Materials->GetMaterial(subMesh.MaterialIndex);
  wgpuRenderPassEncoderSetBindGroup(renderCommandBuffer, 1, material->GetBinding(1), 0, 0);

  wgpuRenderPassEncoderDrawIndexed(renderCommandBuffer, subMesh.IndexCount, instanceCount, subMesh.BaseIndex, subMesh.BaseVertex, 0);
}

void Render::SubmitFullscreenQuad(WGPURenderPassEncoder& renderCommandBuffer, WGPURenderPipeline pipeline) {
  wgpuRenderPassEncoderSetPipeline(renderCommandBuffer, pipeline);

  wgpuRenderPassEncoderSetVertexBuffer(renderCommandBuffer,
                                       0,
                                       s_Data->QuadVertexBuffer->Buffer,
                                       0,
                                       s_Data->QuadVertexBuffer->Size);

  wgpuRenderPassEncoderSetIndexBuffer(renderCommandBuffer,
                                      s_Data->QuadIndexBuffer->Buffer,
                                      WGPUIndexFormat_Uint32,
                                      0,
                                      s_Data->QuadIndexBuffer->Size);
  wgpuRenderPassEncoderDrawIndexed(renderCommandBuffer, 6, 1, 0, 0, 0);
}

void Render::AddShaderDependency(Ref<Shader> shader, Ref<Material> material) {
}

Ref<Texture> Render::GetCurrentSwapChainTexture() {
  static auto textureRef = CreateRef<Texture>();

  WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(m_swapChain);
  textureRef->View = nextTexture;

  return textureRef;
}
