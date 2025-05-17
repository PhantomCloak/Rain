#include "Render.h"
#include "Mesh.h"
#include "ResourceManager.h"
#include "core/Assert.h"
#include "core/Ref.h"
#include "debug/Profiler.h"
#include "render/ShaderManager.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

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
  std::vector<RenderPipeline*> Pipelines;
  std::vector<Material*> Materials;
};

static std::map<std::string, ShaderDependencies> s_ShaderDependencies;

static WGPURendererData* s_Data = nullptr;

// extern "C" WGPUSurface glfwGetWGPUSurface(WGPUInstance instance, GLFWwindow* window);

#include "glfw3webgpu.h"

struct UserData {
  WGPUAdapter adapter = nullptr;
  bool requestEnded = false;
};

struct AdapterRequestData {
  WGPUAdapter adapter = nullptr;
  bool requestEnded = false;
};

struct DeviceRequestData {
  WGPUDevice device = nullptr;
  bool requestEnded = false;
};

struct ComputeDispatchInfo {
  uint32_t workgroupsX;
  uint32_t workgroupsY;
  uint32_t workgroupsZ;
  uint32_t workgroupSize;
};

inline ComputeDispatchInfo CalculateDispatchInfo(
    uint32_t width,
    uint32_t height,
    uint32_t depth = 1,
    uint32_t workgroupSize = 32) {
  ComputeDispatchInfo info;
  info.workgroupSize = workgroupSize;
  info.workgroupsX = (width + workgroupSize - 1) / workgroupSize;
  info.workgroupsY = (height + workgroupSize - 1) / workgroupSize;
  info.workgroupsZ = depth;

  return info;
}

inline WGPUStringView MakeLabel(const char* str) {
  return {str, strlen(str)};
}

// For std::string if needed
inline WGPUStringView MakeLabel(const std::string& str) {
  return {str.c_str(), str.size()};
}

void onAdapterRequestEnded(
    WGPURequestAdapterStatus status,
    WGPUAdapter adapter,
    WGPUStringView message,  // Changed back to WGPUStringView
    void* userdata1,
    void* userdata2) {
  auto* requestData = static_cast<AdapterRequestData*>(userdata1);
  if (status == WGPURequestAdapterStatus_Success) {
    requestData->adapter = adapter;
  } else {
    std::string errorMessage(message.data, message.length);
    std::cerr << "Failed to request adapter: " << errorMessage << std::endl;
  }
  requestData->requestEnded = true;
}

bool Render::Init(void* window) {
  Instance = this;

  WGPUDeviceLostCallback deviceLostCb = [](WGPUDevice const* device,
                                           WGPUDeviceLostReason reason,
                                           WGPUStringView message,
                                           void* userdata1,
                                           void* userdata2) {
    fprintf(stderr, "GPU device lost: %.*s\n", (int)message.length, message.data);
  };

  WGPUUncapturedErrorCallback errorCallback = [](WGPUDevice const* device,
                                                 WGPUErrorType type,
                                                 WGPUStringView message,
                                                 void* userdata1,
                                                 void* userdata2) {
    const char* errorTypeName = "";
    switch (type) {
      case WGPUErrorType_Validation:
        errorTypeName = "Validation";
        break;
      case WGPUErrorType_OutOfMemory:
        errorTypeName = "Out of Memory";
        break;
        break;
      case WGPUErrorType_Internal:
        errorTypeName = "Internal";
        break;
      default:
        errorTypeName = "Unknown";
        break;
    }
    fprintf(stderr, "Dawn uncaptured error (%s): %.*s\n",
            errorTypeName,
            (int)message.length,
            message.data);
    exit(0);
  };

  WGPULoggingCallback callback = [](WGPULoggingType type, WGPUStringView message, void* userdata1, void* userdata2) {
    switch (type) {
      case WGPULoggingType_Error:
        fprintf(stderr, "Dawn error: %.*s\n", static_cast<int>(message.length), message.data);
        exit(0);
        break;
      case WGPULoggingType_Warning:
        fprintf(stderr, "Dawn warning: %.*s\n", static_cast<int>(message.length), message.data);
        break;
      case WGPULoggingType_Info:
        fprintf(stdout, "Dawn info: %.*s\n", static_cast<int>(message.length), message.data);
        break;
      default:
        fprintf(stdout, "Dawn log: %.*s\n", static_cast<int>(message.length), message.data);
        break;
    }
  };

  WGPUDawnTogglesDescriptor dawnToggles;
  ZERO_INIT(dawnToggles);
  dawnToggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
  dawnToggles.enabledToggles = (const char*[]){
      "chromium_disable_uniformity_analysis",
      "allow_unsafe_apis"};
  dawnToggles.enabledToggleCount = 2;
  dawnToggles.disabledToggleCount = 0;

  WGPUInstanceDescriptor* instanceDesc = ZERO_ALLOC(WGPUInstanceDescriptor);
  instanceDesc->nextInChain = nullptr;
  instanceDesc->nextInChain = &dawnToggles.chain;
  instanceDesc->features.timedWaitAnyEnable = 0;
  instanceDesc->features.timedWaitAnyMaxCount = 64;

  static const WGPUInstance gpuInstance = wgpuCreateInstance(instanceDesc);

  m_window = static_cast<GLFWwindow*>(window);
  m_surface = glfwGetWGPUSurface(gpuInstance, m_window);

  AdapterRequestData requestData{};
  ZERO_INIT(requestData);

  WGPURequestAdapterCallbackInfo callbackInfo;
  ZERO_INIT(callbackInfo);
  callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents,
  callbackInfo.callback = onAdapterRequestEnded,
  callbackInfo.userdata1 = &requestData;

  WGPURequestAdapterOptions adapterOpts;
  ZERO_INIT(adapterOpts);
  adapterOpts.compatibleSurface = m_surface;
  adapterOpts.powerPreference = WGPUPowerPreference_HighPerformance;

  wgpuInstanceRequestAdapter(gpuInstance, &adapterOpts, callbackInfo);

  while (!requestData.requestEnded) {
    wgpuInstanceProcessEvents(gpuInstance);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  WGPUAdapter adapter = requestData.adapter;

  WGPURequiredLimits* requiredLimits = GetRequiredLimits(adapter);

  WGPUUncapturedErrorCallbackInfo uncapturedErrorInfo;
  ZERO_INIT(uncapturedErrorInfo);
  uncapturedErrorInfo.callback = errorCallback;

  WGPUDeviceLostCallbackInfo deviceLostInfo;
  ZERO_INIT(deviceLostInfo);
  deviceLostInfo.mode = WGPUCallbackMode_AllowProcessEvents;
  deviceLostInfo.callback = deviceLostCb;

  auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status,
                                 WGPUDevice device,
                                 WGPUStringView message,
                                 void* userdata1,
                                 void* userdata2) {
    auto* userData = static_cast<DeviceRequestData*>(userdata1);
    RN_CORE_ASSERT(status == WGPURequestDeviceStatus_Success,
                   "An error occurred while acquiring WebGPU device");
    userData->device = device;
    userData->requestEnded = true;
  };

  DeviceRequestData deviceRequestData{};
  WGPURequestDeviceCallbackInfo deviceCallbackInfo;
  ZERO_INIT(deviceCallbackInfo);
  deviceCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
  deviceCallbackInfo.callback = onDeviceRequestEnded;
  deviceCallbackInfo.userdata1 = &deviceRequestData;

  WGPUDeviceDescriptor gpuDeviceDescriptor;
  ZERO_INIT(gpuDeviceDescriptor);
  gpuDeviceDescriptor.label = MakeLabel("MyDevice");
  gpuDeviceDescriptor.requiredFeatures = (WGPUFeatureName[]){
      WGPUFeatureName_TimestampQuery,
      WGPUFeatureName_TextureCompressionBC,
      WGPUFeatureName_Float32Filterable,
      WGPUFeatureName_DepthClipControl};
  gpuDeviceDescriptor.requiredFeatureCount = 4;
  gpuDeviceDescriptor.requiredLimits = requiredLimits;
  gpuDeviceDescriptor.defaultQueue.label = MakeLabel("defq");
  gpuDeviceDescriptor.deviceLostCallbackInfo = deviceLostInfo;
  gpuDeviceDescriptor.uncapturedErrorCallbackInfo = uncapturedErrorInfo;

  wgpuAdapterRequestDevice(adapter, &gpuDeviceDescriptor, deviceCallbackInfo);

  while (!deviceRequestData.requestEnded) {
    wgpuInstanceProcessEvents(gpuInstance);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  m_device = deviceRequestData.device;

  m_queue = wgpuDeviceGetQueue(m_device);
  RN_CORE_ASSERT(m_queue, "An error occurred while acquiring the queue. This might indicate unsupported browser/device.");

  m_RenderContext = CreateRef<RenderContext>(adapter, m_surface, m_device, m_queue);

  WGPUSurfaceCapabilities capabilities;
  wgpuSurfaceGetCapabilities(m_surface, adapter, &capabilities);

  m_swapChainFormat = WGPUTextureFormat_BGRA8Unorm;

  int width, height;
  glfwGetFramebufferSize(m_window, &width, &height);
  ConfigureSurface(width, height);

  // WGPULoggingCallbackInfo callbackInfo = {};
  // ZERO_INIT(callbackInfo);
  // callbackInfo.callback = callback;

  // wgpuDeviceSetLoggingCallback(m_device, callbackInfo);

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

const char* getAdapterTypeString(WGPUAdapterType adapterType) {
  switch (adapterType) {
    case WGPUAdapterType_DiscreteGPU:
      return "Discrete GPU";
    case WGPUAdapterType_IntegratedGPU:
      return "Integrated GPU";
    case WGPUAdapterType_CPU:
      return "CPU";
    case WGPUAdapterType_Unknown:
      return "Unknown";
    default:
      return "Unknown";
  }
}

const char* getBackendTypeString(WGPUBackendType backendType) {
  switch (backendType) {
    case WGPUBackendType_Null:
      return "Null";
    case WGPUBackendType_WebGPU:
      return "WebGPU";
    case WGPUBackendType_D3D11:
      return "Direct3D 11";
    case WGPUBackendType_D3D12:
      return "Direct3D 12";
    case WGPUBackendType_Metal:
      return "Metal";
    case WGPUBackendType_Vulkan:
      return "Vulkan";
    case WGPUBackendType_OpenGL:
      return "OpenGL";
    case WGPUBackendType_OpenGLES:
      return "OpenGLES";
    default:
      return "Unknown";
  }
}

WGPURequiredLimits* Render::GetRequiredLimits(WGPUAdapter adapter) {
  static WGPUSupportedLimits supportedLimits = {};
  supportedLimits.nextInChain = nullptr;

#ifdef __EMSCRIPTEN__
  // Specific limits required for Emscripten
  supportedLimits.limits.minStorageBufferOffsetAlignment = 256;
  supportedLimits.limits.minUniformBufferOffsetAlignment = 256;
#else
  wgpuAdapterGetLimits(adapter, &supportedLimits);
#endif

  WGPURequiredLimits* requiredLimits = (WGPURequiredLimits*)malloc(sizeof(WGPURequiredLimits));
  requiredLimits->limits = supportedLimits.limits;  // Copy all supported limits
  m_Limits = supportedLimits.limits;

  // Override specific limits as needed
  requiredLimits->limits.maxBufferSize = 150000 * sizeof(VertexAttribute);
  requiredLimits->limits.maxVertexBufferArrayStride = sizeof(VertexAttribute);  // Ensure only one assignment
  requiredLimits->limits.maxTextureDimension1D = 4096;
  requiredLimits->limits.maxTextureDimension2D = 4096;

#ifndef __EMSCRIPTEN__
  requiredLimits->limits.maxTextureDimension1D = 6098;
  requiredLimits->limits.maxTextureDimension2D = 6098;
#endif
  // Add any additional specific overrides below

  RN_LOG("Max Texture Limit: {}", supportedLimits.limits.maxTextureDimension2D);
  requiredLimits->nextInChain = nullptr;
  return requiredLimits;
}

void Render::ConfigureSurface(uint32_t width, uint32_t height) {
  WGPUSurfaceConfiguration config = {};
  config.device = m_device;
  config.format = m_swapChainFormat;
  config.usage = WGPUTextureUsage_RenderAttachment;
  config.alphaMode = WGPUCompositeAlphaMode_Auto;
  config.width = width;
  config.height = height;
  config.presentMode = WGPUPresentMode_Fifo;
  config.viewFormatCount = 1;
  config.viewFormats = &m_swapChainFormat;

  wgpuSurfaceConfigure(m_surface, &config);
}

WGPUTextureView Render::GetCurrentTextureView() {
  WGPUSurfaceTexture surfaceTexture = {};
  wgpuSurfaceGetCurrentTexture(m_surface, &surfaceTexture);

  if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
    switch (surfaceTexture.status) {
      case WGPUSurfaceGetCurrentTextureStatus_Lost:
        // Reconfigure surface here
        // ConfigureSurface(m_swapChainDesc.width, m_swapChainDesc.height);
        wgpuSurfaceGetCurrentTexture(m_surface, &surfaceTexture);
        break;
      case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
        RN_CORE_ASSERT(false, "Out of memory when acquiring next swapchain texture");
        return nullptr;
      case WGPUSurfaceGetCurrentTextureStatus_DeviceLost:
        RN_CORE_ASSERT(false, "Device lost when acquiring next swapchain texture");
        return nullptr;
      default:
        RN_CORE_ASSERT(false, "Unknown error when acquiring next swapchain texture");
        return nullptr;
    }
  }

  WGPUTextureViewDescriptor viewDesc = {};
  viewDesc.format = m_swapChainFormat;
  viewDesc.dimension = WGPUTextureViewDimension_2D;
  viewDesc.mipLevelCount = 1;
  viewDesc.arrayLayerCount = 1;
  viewDesc.aspect = WGPUTextureAspect_All;

  return wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);
  // Don't call wgpuSurfacePresent here, do it after rendering is complete
}

Ref<Texture2D> Render::GetWhiteTexture() {
  static auto whiteTexture = Rain::ResourceManager::GetTexture("T_Default");
  RN_ASSERT(whiteTexture->GetReadableView() != 0, "Material: Default texture couldn't found.");
  return whiteTexture;
}

Ref<Sampler> Render::GetDefaultSampler() {
  static auto sampler = Sampler::Create(Sampler::GetDefaultProps("S_DefaultSampler"));
  RN_ASSERT(sampler != 0, "Material: Default sampler couldn't found.");
  return sampler;
}

void Render::ComputeMip(Texture2D* input) {
  auto dir = RESOURCE_DIR "/shaders/ComputeMip.wgsl";
  static Ref<Shader> computeShader = ShaderManager::LoadShader("SH_Compute", dir);
  auto device = RenderContext::GetDevice();

  // Prepare pipeline layout
  std::vector<WGPUBindGroupLayout> bindGroupLayouts;
  for (const auto& [_, layout] : computeShader->GetReflectionInfo().LayoutDescriptors) {
    bindGroupLayouts.push_back(layout);
  }

  WGPUPipelineLayoutDescriptor layoutDesc = {};
  layoutDesc.bindGroupLayoutCount = bindGroupLayouts.size();
  layoutDesc.bindGroupLayouts = bindGroupLayouts.data();
  WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &layoutDesc);

  // Prepare compute pipeline
  WGPUComputePipelineDescriptor computePipelineDesc = {};
  computePipelineDesc.compute.constantCount = 0;
  computePipelineDesc.compute.constants = nullptr;
  computePipelineDesc.compute.entryPoint = MakeLabel("computeMipMap");
  computePipelineDesc.compute.module = computeShader->GetNativeShaderModule();
  computePipelineDesc.layout = pipelineLayout;
  WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &computePipelineDesc);

  int mipCount = RenderUtils::CalculateMipCount(input->GetSpec().Width, input->GetSpec().Height);

  for (uint32_t mipLevel = 1; mipLevel < mipCount; ++mipLevel) {
    auto encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);

    WGPUComputePassDescriptor computePassDesc = {};
    WGPUComputePassEncoder computePass = wgpuCommandEncoderBeginComputePass(encoder, &computePassDesc);
    wgpuComputePassEncoderSetPipeline(computePass, pipeline);

    WGPUBindGroupEntry bindGroupEntries[2] = {
        {.binding = 0, .textureView = input->GetReadableView(mipLevel - 1)},
        {.binding = 1, .textureView = input->GetWriteableView(mipLevel)}};

    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout = bindGroupLayouts[0];
    bindGroupDesc.entryCount = 2;
    bindGroupDesc.entries = bindGroupEntries;
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);

    const uint32_t workgroupSize = 32;
    const uint32_t workgroupsX = (std::max(input->GetSpec().Width >> mipLevel, (uint32_t)1) + workgroupSize - 1) / workgroupSize;
    const uint32_t workgroupsY = (std::max(input->GetSpec().Height >> mipLevel, (uint32_t)1) + workgroupSize - 1) / workgroupSize;

    wgpuComputePassEncoderSetBindGroup(computePass, 0, bindGroup, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroupsX, workgroupsY, 1);
    wgpuComputePassEncoderEnd(computePass);

    WGPUCommandBufferDescriptor cmdBufferDesc = {};
    WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);
    wgpuQueueSubmit(*RenderContext::GetQueue(), 1, &commandBuffer);

    wgpuBindGroupRelease(bindGroup);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(commandBuffer);
  }

  wgpuPipelineLayoutRelease(pipelineLayout);
  wgpuComputePipelineRelease(pipeline);
}

void Render::ComputeMipCube(TextureCube* input) {
  auto computeShader = ShaderManager::LoadShader("SH_ComputeCube", RESOURCE_DIR "/shaders/ComputeMipCube.wgsl");

  auto device = RenderContext::GetDevice();

  auto bindGroupLayout = computeShader->GetReflectionInfo().LayoutDescriptors.begin()->second;
  WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
  pipelineLayoutDesc.bindGroupLayoutCount = 1;
  pipelineLayoutDesc.bindGroupLayouts = &bindGroupLayout;
  const auto pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

  WGPUComputePipelineDescriptor pipelineDesc = {};
  pipelineDesc.layout = pipelineLayout;
  pipelineDesc.compute.entryPoint = MakeLabel("computeMipMap");
  pipelineDesc.compute.module = computeShader->GetNativeShaderModule();
  const auto pipeline = wgpuDeviceCreateComputePipeline(device, &pipelineDesc);

  const uint32_t mipCount = RenderUtils::CalculateMipCount(
      input->GetSpec().Width,
      input->GetSpec().Height);

  for (uint32_t mipLevel = 1; mipLevel < mipCount; ++mipLevel) {
    WGPUTextureViewDescriptor* viewDesc = ZERO_ALLOC(WGPUTextureViewDescriptor);
    viewDesc->nextInChain = nullptr;
    viewDesc->format = RenderTypeUtils::ToRenderType(input->GetFormat());
    viewDesc->dimension = WGPUTextureViewDimension_2DArray;
    viewDesc->baseMipLevel = mipLevel - 1;
    viewDesc->mipLevelCount = 1;
    viewDesc->baseArrayLayer = 0;
    viewDesc->arrayLayerCount = 6;

    auto inputView = wgpuTextureCreateView(input->m_TextureBuffer, viewDesc);

    WGPUBindGroupEntry bindGroupEntries[2] = {
        {.binding = 0, .textureView = inputView},
        {.binding = 1, .textureView = input->GetWriteableView(mipLevel)}};

    WGPUBindGroupDescriptor bindGroupDesc = {};
    ZERO_INIT(bindGroupDesc);
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entryCount = 2;
    bindGroupDesc.entries = bindGroupEntries;
    auto bindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);

    const uint32_t workgroupSize = 32;
    const uint32_t workgroupsX = (std::max(input->GetSpec().Width >> mipLevel, 1u) + workgroupSize - 1) / workgroupSize;
    const uint32_t workgroupsY = (std::max(input->GetSpec().Height >> mipLevel, 1u) + workgroupSize - 1) / workgroupSize;

    auto encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    {
      auto computePass = wgpuCommandEncoderBeginComputePass(encoder, nullptr);
      wgpuComputePassEncoderSetPipeline(computePass, pipeline);
      wgpuComputePassEncoderSetBindGroup(computePass, 0, bindGroup, 0, nullptr);
      wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroupsX, workgroupsY, 6);
      wgpuComputePassEncoderEnd(computePass);
    }

    auto commandBuffer = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(*RenderContext::GetQueue(), 1, &commandBuffer);

    wgpuBindGroupRelease(bindGroup);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(commandBuffer);
  }

  wgpuPipelineLayoutRelease(pipelineLayout);
  wgpuComputePipelineRelease(pipeline);
}

struct PrefilterUniform {
  uint32_t currentMipLevel;
  uint32_t mipLevelCount;
  uint32_t _pad[2];
};

std::pair<Ref<TextureCube>, Ref<TextureCube>> Render::CreateEnvironmentMap(const std::string& filepath) {
  const uint32_t cubemapSize = 2048;
  const uint32_t irradianceMapSize = 32;

  TextureProps cubeProps = {};
  cubeProps.Width = cubemapSize;
  cubeProps.Height = cubemapSize;
  cubeProps.GenerateMips = true;
  cubeProps.Format = TextureFormat::RGBA16F;

  Ref<TextureCube> envUnfiltered = TextureCube::Create(cubeProps);
  Ref<TextureCube> envFiltered = TextureCube::Create(cubeProps);

  cubeProps.Width = irradianceMapSize;
  cubeProps.Height = irradianceMapSize;
  cubeProps.Format = TextureFormat::RGBA32F;
  cubeProps.GenerateMips = false;
  Ref<TextureCube> irradianceMap = TextureCube::Create(cubeProps);

  Ref<Texture2D> envEquirect = Texture2D::Create(TextureProps(), filepath);

  Render::ComputeEquirectToCubemap(envEquirect.get(), envUnfiltered.get());
  Render::ComputeMipCube(envUnfiltered.get());

  Render::ComputeEquirectToCubemap(envEquirect.get(), envFiltered.get());

  Render::PreFilter(envUnfiltered.get(), envFiltered.get());
  Render::ComputeEnvironmentIrradiance(envFiltered.get(), irradianceMap.get());

  return std::make_pair(envFiltered, irradianceMap);
}

void Render::PreFilter(TextureCube* input, TextureCube* output) {
  auto computeShader = ShaderManager::LoadShader("SH_ComputeCube2", RESOURCE_DIR "/shaders/environment_prefilter.wgsl");

  const uint32_t mipCount = RenderUtils::CalculateMipCount(input->GetSpec().Width, input->GetSpec().Height);
  const uint32_t uniformStride = std::max((uint32_t)sizeof(PrefilterUniform), (uint32_t)Instance->m_Limits.minUniformBufferOffsetAlignment);
  auto uniformBuffer = GPUAllocator::GAlloc(WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, uniformStride * mipCount);

  for (uint32_t mipLevel = 0; mipLevel < mipCount; ++mipLevel) {
    PrefilterUniform uniform = {
        .currentMipLevel = mipLevel,
        .mipLevelCount = mipCount};
    uniformBuffer->SetData(&uniform, mipLevel * uniformStride, sizeof(uniform));
  }

  auto sampler = Sampler::Create({.Name = "S_Skybox",
                                  .WrapFormat = TextureWrappingFormat::ClampToEdges,
                                  .MagFilterFormat = FilterMode::Linear,
                                  .MinFilterFormat = FilterMode::Linear,
                                  .MipFilterFormat = FilterMode::Linear});

  auto device = RenderContext::GetDevice();
  const auto bindGroupLayout = computeShader->GetReflectionInfo().LayoutDescriptors.begin()->second;
  WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
  pipelineLayoutDesc.bindGroupLayoutCount = 1;
  pipelineLayoutDesc.bindGroupLayouts = &bindGroupLayout;
  const auto pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

  WGPUComputePipelineDescriptor pipelineDesc = {};
  pipelineDesc.layout = pipelineLayout;
  pipelineDesc.compute.entryPoint = MakeLabel("prefilterCubeMap");
  pipelineDesc.compute.module = computeShader->GetNativeShaderModule();
  const auto pipeline = wgpuDeviceCreateComputePipeline(device, &pipelineDesc);

  for (uint32_t mipLevel = 0; mipLevel < mipCount; ++mipLevel) {
    WGPUBindGroupEntry bindGroupEntries[4] = {
        {.binding = 0, .textureView = input->GetReadableView(0)},  // Always sample from mip 0
        {.binding = 1, .textureView = output->GetWriteableView(mipLevel)},
        {.binding = 2, .sampler = *sampler->GetNativeSampler()},
        {.binding = 3, .buffer = uniformBuffer->Buffer, .offset = 0, .size = sizeof(PrefilterUniform)}};

    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entryCount = 4;
    bindGroupDesc.entries = bindGroupEntries;
    const auto bindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);

    const uint32_t workgroupSize = 32;
    const uint32_t workgroupsX = (std::max(input->GetSpec().Width >> mipLevel, 1u) + workgroupSize - 1) / workgroupSize;
    const uint32_t workgroupsY = (std::max(input->GetSpec().Height >> mipLevel, 1u) + workgroupSize - 1) / workgroupSize;

    auto encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    {
      auto computePass = wgpuCommandEncoderBeginComputePass(encoder, nullptr);
      uint32_t dynamicOffset = mipLevel * uniformStride;

      wgpuComputePassEncoderSetPipeline(computePass, pipeline);
      wgpuComputePassEncoderSetBindGroup(computePass, 0, bindGroup, 1, &dynamicOffset);
      wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroupsX, workgroupsY, 6);
      wgpuComputePassEncoderEnd(computePass);
    }

    const auto commandBuffer = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(*RenderContext::GetQueue(), 1, &commandBuffer);

    wgpuBindGroupRelease(bindGroup);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(commandBuffer);
  }

  wgpuPipelineLayoutRelease(pipelineLayout);
  wgpuComputePipelineRelease(pipeline);
}

void Render::ComputeEnvironmentIrradiance(TextureCube* input, TextureCube* output) {
  auto computeShader = ShaderManager::LoadShader("SH_EnvironmentIrradiance", RESOURCE_DIR "/shaders/environment_irradiance.wgsl");

  auto sampler = Sampler::Create({.Name = "S_Skybox",
                                  .WrapFormat = TextureWrappingFormat::ClampToEdges,
                                  .MagFilterFormat = FilterMode::Linear,
                                  .MinFilterFormat = FilterMode::Linear,
                                  .MipFilterFormat = FilterMode::Linear});

  const auto device = RenderContext::GetDevice();
  const auto bindGroupLayout = computeShader->GetReflectionInfo().LayoutDescriptors.begin()->second;

  WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
  pipelineLayoutDesc.bindGroupLayoutCount = 1;
  pipelineLayoutDesc.bindGroupLayouts = &bindGroupLayout;
  const auto pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

  WGPUBindGroupEntry bindGroupEntries[3] = {
      {.binding = 0, .textureView = output->GetWriteableView(0)},
      {.binding = 1, .textureView = input->GetReadableView(0)},
      {.binding = 2, .sampler = *sampler->GetNativeSampler()}};

  WGPUBindGroupDescriptor bindGroupDesc = {};
  bindGroupDesc.layout = bindGroupLayout;
  bindGroupDesc.entryCount = 3;
  bindGroupDesc.entries = bindGroupEntries;
  const auto bindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);

  WGPUComputePipelineDescriptor pipelineDesc = {};
  pipelineDesc.layout = pipelineLayout;
  pipelineDesc.compute.module = computeShader->GetNativeShaderModule();
  pipelineDesc.compute.entryPoint = MakeLabel("prefilterCubeMap");
  const auto pipeline = wgpuDeviceCreateComputePipeline(device, &pipelineDesc);

  const uint32_t workgroupSize = 32;
  const uint32_t workgroupsX = (input->GetSpec().Width + workgroupSize - 1) / workgroupSize;
  const uint32_t workgroupsY = (input->GetSpec().Height + workgroupSize - 1) / workgroupSize;

  auto encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
  {
    auto computePass = wgpuCommandEncoderBeginComputePass(encoder, nullptr);
    wgpuComputePassEncoderSetPipeline(computePass, pipeline);
    wgpuComputePassEncoderSetBindGroup(computePass, 0, bindGroup, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroupsX, workgroupsY, 6);
    wgpuComputePassEncoderEnd(computePass);
  }

  auto commandBuffer = wgpuCommandEncoderFinish(encoder, nullptr);
  wgpuQueueSubmit(*RenderContext::GetQueue(), 1, &commandBuffer);

  // Cleanup WebGPU resources
  wgpuBindGroupRelease(bindGroup);
  wgpuPipelineLayoutRelease(pipelineLayout);
  wgpuComputePipelineRelease(pipeline);
  wgpuCommandEncoderRelease(encoder);
  wgpuCommandBufferRelease(commandBuffer);
}

WGPURenderPassEncoder Render::BeginRenderPass(Ref<RenderPass> pass, WGPUCommandEncoder& encoder) {
  RN_PROFILE_FUNC;
  pass->Prepare();

  auto renderFrameBuffer = pass->GetTargetFrameBuffer();

  WGPUTextureView swp;
  WGPURenderPassDescriptor passDesc = {};
  ZERO_INIT(passDesc);

  passDesc.nextInChain = nullptr;
  passDesc.label = MakeLabel(pass->GetProps().DebugName);

  WGPURenderPassColorAttachment colorAttachment{};
  ZERO_INIT(colorAttachment);

  colorAttachment.nextInChain = nullptr;
  colorAttachment.loadOp = WGPULoadOp_Load;
  colorAttachment.storeOp = WGPUStoreOp_Store;
  colorAttachment.clearValue = WGPUColor{0, 0, 0, 1};
  colorAttachment.resolveTarget = nullptr;

  // #if __EMSCRIPTEN__
  colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
  // #endif

  if (renderFrameBuffer->HasColorAttachment()) {
    if (renderFrameBuffer->m_FrameBufferSpec.SwapChainTarget) {
      Instance->m_SwapTexture = Render::Instance->GetCurrentSwapChainTexture();
      colorAttachment.resolveTarget = Instance->m_SwapTexture;
      // colorAttachment.view = Instance->m_SwapTexture;
    }

    colorAttachment.view = renderFrameBuffer->GetAttachment(0)->GetReadableView();
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAttachment;
  }

  if (renderFrameBuffer->HasDepthAttachment()) {
    WGPURenderPassDepthStencilAttachment depthAttachment = {};
    ZERO_INIT(depthAttachment);

    auto depth = renderFrameBuffer->GetDepthAttachment();
    if (depth->m_ReadViews.size() > 1) {
      int layerNum = renderFrameBuffer->m_FrameBufferSpec.ExistingImageLayers[0];
      depthAttachment.view = renderFrameBuffer->GetDepthAttachment()->m_ReadViews[layerNum + 1];
    } else {
      depthAttachment.view = renderFrameBuffer->GetDepthAttachment()->GetReadableView();
    }

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

  for (const auto& [index, bindGroup] : bindings->GetBindGroups()) {
    wgpuRenderPassEncoderSetBindGroup(renderPass, index, bindGroup, 0, 0);
  }

  return renderPass;
}

void Render::EndRenderPass(Ref<RenderPass> pass, WGPURenderPassEncoder& encoder) {
  RN_PROFILE_FUNC;
  wgpuRenderPassEncoderEnd(encoder);
  wgpuRenderPassEncoderRelease(encoder);

  const auto renderFrameBuffer = pass->GetTargetFrameBuffer();
  if (renderFrameBuffer->m_FrameBufferSpec.SwapChainTarget) {
    wgpuTextureViewRelease(Instance->m_SwapTexture);
  }
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

void Render::ComputeEquirectToCubemap(Texture2D* equirectTexture, TextureCube* outputCubemap) {
  auto computeShader = ShaderManager::LoadShader("SH_EquirectToCubemap", RESOURCE_DIR "/shaders/equirectangular_to_cubemap.wgsl");

  auto sampler = Sampler::Create({.Name = "S_EquirectSampler",
                                  .WrapFormat = TextureWrappingFormat::ClampToEdges,
                                  .MagFilterFormat = FilterMode::Linear,
                                  .MinFilterFormat = FilterMode::Linear,
                                  .MipFilterFormat = FilterMode::Linear});

  auto device = RenderContext::GetDevice();

  // Create pipeline layout
  auto bindGroupLayout = computeShader->GetReflectionInfo().LayoutDescriptors.begin()->second;
  WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
  pipelineLayoutDesc.bindGroupLayoutCount = 1;
  pipelineLayoutDesc.bindGroupLayouts = &bindGroupLayout;
  auto pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

  WGPUBindGroupEntry bindGroupEntries[3] = {
      {.binding = 0, .textureView = outputCubemap->GetWriteableView(0)},
      {.binding = 1, .textureView = equirectTexture->GetReadableView(0)},
      {.binding = 2, .sampler = *sampler->GetNativeSampler()}};
  WGPUBindGroupDescriptor bindGroupDesc = {};
  bindGroupDesc.layout = bindGroupLayout;
  bindGroupDesc.entryCount = 3;
  bindGroupDesc.entries = bindGroupEntries;
  auto bindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);

  WGPUComputePipelineDescriptor pipelineDesc = {};
  pipelineDesc.layout = pipelineLayout;
  pipelineDesc.compute.module = computeShader->GetNativeShaderModule();
  pipelineDesc.compute.entryPoint = MakeLabel("main");
  auto pipeline = wgpuDeviceCreateComputePipeline(device, &pipelineDesc);

  const uint32_t workgroupSize = 32;
  const uint32_t workgroupsX = (outputCubemap->GetSpec().Width + workgroupSize - 1) / workgroupSize;
  const uint32_t workgroupsY = (outputCubemap->GetSpec().Height + workgroupSize - 1) / workgroupSize;

  auto encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
  {
    auto computePass = wgpuCommandEncoderBeginComputePass(encoder, nullptr);
    wgpuComputePassEncoderSetPipeline(computePass, pipeline);
    wgpuComputePassEncoderSetBindGroup(computePass, 0, bindGroup, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroupsX, workgroupsY, 6);
    wgpuComputePassEncoderEnd(computePass);
  }
  auto commandBuffer = wgpuCommandEncoderFinish(encoder, nullptr);
  wgpuQueueSubmit(*RenderContext::GetQueue(), 1, &commandBuffer);

  // Cleanup WebGPU resources
  wgpuBindGroupRelease(bindGroup);
  wgpuPipelineLayoutRelease(pipelineLayout);
  wgpuComputePipelineRelease(pipeline);
  wgpuCommandEncoderRelease(encoder);
  wgpuCommandBufferRelease(commandBuffer);
}

void Render::RegisterShaderDependency(Ref<Shader> shader, Material* material) {
  s_ShaderDependencies[shader->GetName()].Materials.push_back(material);
}

void Render::RegisterShaderDependency(Ref<Shader> shader, RenderPipeline* material) {
  s_ShaderDependencies[shader->GetName()].Pipelines.push_back(material);
}

void Render::ReloadShader(Ref<Shader> shader) {
  auto dependencies = s_ShaderDependencies[shader->GetName()];
  for (auto& material : dependencies.Materials) {
    material->OnShaderReload();
  }

  for (auto& pipeline : dependencies.Pipelines) {
    pipeline->Invalidate();
  }
}

WGPUTextureView Render::GetCurrentSwapChainTexture() {
  return GetCurrentTextureView();
}
