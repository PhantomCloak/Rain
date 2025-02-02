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
WGPUInstance Render::CreateGPUInstance() {
  WGPUInstanceDescriptor* instanceDesc = ZERO_ALLOC(WGPUInstanceDescriptor);
  instanceDesc->nextInChain = nullptr;

#if !__EMSCRIPTEN__
  static std::vector<const char*> enabledToggles = {
      "chromium_disable_uniformity_analysis",
      "allow_unsafe_apis"};

  WGPUDawnTogglesDescriptor dawnToggleDesc = {};
  dawnToggleDesc.chain.next = nullptr;
  dawnToggleDesc.chain.sType = WGPUSType_DawnTogglesDescriptor;

  dawnToggleDesc.enabledToggles = enabledToggles.data();
  dawnToggleDesc.enabledToggleCount = enabledToggles.size();
  dawnToggleDesc.disabledToggleCount = 0;

  instanceDesc->nextInChain = &dawnToggleDesc.chain;

  instanceDesc->features.timedWaitAnyEnable = 0;
  instanceDesc->features.timedWaitAnyMaxCount = 64;
#endif

#if !__EMSCRIPTEN__
  m_Instance = wgpuCreateInstance(instanceDesc);
#else
  m_Instance = wgpuCreateInstance(nullptr);
#endif

  RN_CORE_ASSERT(m_Instance, "An error occured while acquiring the WebGPU instance.");

  return m_Instance;
}

inline WGPUStringView MakeLabel(const char* str) {
  return {str, strlen(str)};
}

// For std::string if needed
inline WGPUStringView MakeLabel(const std::string& str) {
  return {str.c_str(), str.size()};
}

bool Render::Init(void* window) {
    Instance = this;
    
    // Instance creation
    RN_LOG("Creating GPU Instance");
    CreateGPUInstance();
    RN_LOG("GPU Instance: {}", m_Instance == nullptr);
    
    // Window and surface setup
    m_window = static_cast<GLFWwindow*>(window);
    RN_LOG("Window: {}", m_window == nullptr);
    
#if EMSCRIPTEN
    m_surface = htmlGetCanvasSurface(m_Instance, "canvas");
#else
    m_surface = glfwGetWGPUSurface(m_Instance, m_window);
#endif
    RN_LOG("GPU Surface {}", m_surface == nullptr);

    // Adapter request
    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.compatibleSurface = m_surface;
    adapterOpts.powerPreference = WGPUPowerPreference_HighPerformance;
    adapterOpts.nextInChain = nullptr;
    m_adapter = RequestAdapter(m_Instance, &adapterOpts);
    RN_LOG("GPU Adapter {}", m_adapter == nullptr);

    // Device setup
    WGPURequiredLimits* requiredLimits = GetRequiredLimits(m_adapter);
    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.label = MakeLabel("MyDevice");

	
   WGPUDeviceLostCallback deviceLostCb = [](WGPUDevice const* device,
                                            WGPUDeviceLostReason reason,
                                            WGPUStringView message,
                                            void* userdata1,
                                            void* userdata2) {
        fprintf(stderr, "GPU device lost: %.*s\n", (int)message.length, message.data);
    };

    WGPUDeviceLostCallbackInfo deviceLostInfo = {};
    deviceLostInfo.nextInChain = nullptr;
    deviceLostInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    deviceLostInfo.callback = deviceLostCb;
    deviceLostInfo.userdata1 = nullptr;
    deviceLostInfo.userdata2 = nullptr;



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

// Set up the uncaptured error callback info
WGPUUncapturedErrorCallbackInfo uncapturedErrorInfo = {};
uncapturedErrorInfo.nextInChain = nullptr;
uncapturedErrorInfo.callback = errorCallback;
uncapturedErrorInfo.userdata1 = nullptr;
uncapturedErrorInfo.userdata2 = nullptr;

    
#if EMSCRIPTEN
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredFeatures = nullptr;
#else
    static WGPUFeatureName requiredFeatures[] = {
        WGPUFeatureName_TimestampQuery,
        WGPUFeatureName_TextureCompressionBC,
        WGPUFeatureName_DepthClipControl
    };
    deviceDesc.requiredFeatures = requiredFeatures;
    deviceDesc.requiredFeatureCount = 3;
#endif

    deviceDesc.requiredLimits = requiredLimits;
    deviceDesc.defaultQueue.label = MakeLabel("defq");
    deviceDesc.nextInChain = nullptr;
	deviceDesc.deviceLostCallbackInfo = deviceLostInfo;  // Add the device lost callback
	deviceDesc.uncapturedErrorCallbackInfo = uncapturedErrorInfo;  // Add the device lost callback
    
    m_device = RequestDevice(m_adapter, &deviceDesc);
    m_queue = wgpuDeviceGetQueue(m_device);
    RN_CORE_ASSERT(m_queue, "An error occurred while acquiring the queue. This might indicate unsupported browser/device.");

    // Render context setup
    m_RenderContext = CreateRef<RenderContext>(m_adapter, m_surface, m_device, m_queue);

    // Surface configuration
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    // Get surface capabilities
    WGPUSurfaceCapabilities capabilities;
    wgpuSurfaceGetCapabilities(m_surface, m_adapter, &capabilities);

    // Select format
    WGPUTextureFormat swapChainFormat;
#if EMSCRIPTEN
    RN_ASSERT(capabilities.formatCount > 0, "No supported formats for surface");
    swapChainFormat = capabilities.formats[0];
#else
    swapChainFormat = WGPUTextureFormat_BGRA8Unorm;
#endif
    m_swapChainFormat = swapChainFormat;

    // Configure surface instead of creating swapchain
    WGPUSurfaceConfiguration surfaceConfig = {};
    surfaceConfig.nextInChain = nullptr;
    surfaceConfig.device = m_device;
    surfaceConfig.format = swapChainFormat;
    surfaceConfig.usage = WGPUTextureUsage_RenderAttachment;
    surfaceConfig.alphaMode = WGPUCompositeAlphaMode_Auto;
    surfaceConfig.width = width;
    surfaceConfig.height = height;
    surfaceConfig.presentMode = WGPUPresentMode_Fifo;
    surfaceConfig.viewFormatCount = 1;
    surfaceConfig.viewFormats = &swapChainFormat;

    // Configure the surface
    wgpuSurfaceConfigure(m_surface, &surfaceConfig);

    // Setup logging callback
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


    WGPULoggingCallbackInfo callbackInfo = {};
    callbackInfo.nextInChain = nullptr;
    callbackInfo.callback = callback;
    callbackInfo.userdata1 = nullptr;
    callbackInfo.userdata2 = nullptr;

    wgpuDeviceSetLoggingCallback(m_device, callbackInfo);
    
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

struct UserData {
  WGPUAdapter adapter = nullptr;
  bool requestEnded = false;
};

struct AdapterRequestData {
  WGPUAdapter adapter = nullptr;
  bool requestEnded = false;
};

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
        // Convert WGPUStringView to string for logging
			std::string errorMessage(message.data, message.length);
        std::cerr << "Failed to request adapter: " << errorMessage << std::endl;
    }
    requestData->requestEnded = true;
}

WGPUAdapter Render::RequestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions* options) {
    AdapterRequestData requestData{};
    
    WGPURequestAdapterCallbackInfo callbackInfo{
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = onAdapterRequestEnded,
        .userdata1 = &requestData,
        .userdata2 = nullptr
    };

    WGPUFuture future = wgpuInstanceRequestAdapter(instance, options, callbackInfo);
    
    while (!requestData.requestEnded) {
		wgpuInstanceProcessEvents(instance);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    assert(requestData.adapter != nullptr && "Failed to acquire WebGPU adapter");
    return requestData.adapter;
}

WGPUDevice Render::RequestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor) {
    struct UserData {
        WGPUDevice device = nullptr;
        bool requestEnded = false;
    };
    UserData userData{};

    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status,
                                  WGPUDevice device,
                                  WGPUStringView message,
                                  void* userdata1,
                                  void* userdata2) {
        RN_LOG("Device callback received");
        auto* userData = static_cast<UserData*>(userdata1);
        RN_CORE_ASSERT(status == WGPURequestDeviceStatus_Success, 
                      "An error occurred while acquiring WebGPU device");
        userData->device = device;
        userData->requestEnded = true;
    };

    WGPURequestDeviceCallbackInfo callbackInfo{
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_AllowProcessEvents,
        .callback = onDeviceRequestEnded,
        .userdata1 = &userData,
        .userdata2 = nullptr
    };

    WGPUFuture future = wgpuAdapterRequestDevice(adapter, descriptor, callbackInfo);

    // Wait for the callback
#if EMSCRIPTEN
    while (!userData.requestEnded) {
        emscripten_sleep(1000);
    }
#else
    while (!userData.requestEnded) {
		wgpuInstanceProcessEvents(m_Instance);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
#endif

    assert(userData.requestEnded);
    return userData.device;
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
                //ConfigureSurface(m_swapChainDesc.width, m_swapChainDesc.height);
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
  RN_ASSERT(whiteTexture->GetNativeView() != 0, "Material: Default texture couldn't found.");
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
  const uint32_t workgroupSizePerDim = 8;

  for (uint32_t mipLevel = 1; mipLevel < mipCount; ++mipLevel) {
    auto encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);

    WGPUComputePassDescriptor computePassDesc = {};
    WGPUComputePassEncoder computePass = wgpuCommandEncoderBeginComputePass(encoder, &computePassDesc);
    wgpuComputePassEncoderSetPipeline(computePass, pipeline);

    auto textureWidth = std::max(input->GetSpec().Width >> mipLevel, (uint32_t)1);
    auto textureHeight = std::max(input->GetSpec().Height >> mipLevel, (uint32_t)1);
    uint32_t workgroupCountX = (textureWidth + workgroupSizePerDim - 1) / workgroupSizePerDim;
    uint32_t workgroupCountY = (textureHeight + workgroupSizePerDim - 1) / workgroupSizePerDim;

    // Create texture views for input and output mip levels
    WGPUTextureViewDescriptor inputViewDesc = {
        .format = WGPUTextureFormat_RGBA8Unorm,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = mipLevel - 1,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1};
    WGPUTextureView inputTextureView = wgpuTextureCreateView(input->TextureBuffer, &inputViewDesc);

    WGPUTextureViewDescriptor outputViewDesc = {
        .format = WGPUTextureFormat_RGBA8Unorm,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = mipLevel,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1};

    WGPUTextureView outputTextureView = wgpuTextureCreateView(input->TextureBuffer, &outputViewDesc);

    // Create and set bind group
    WGPUBindGroupEntry bindGroupEntries[2] = {
        {.binding = 0, .textureView = inputTextureView},
        {.binding = 1, .textureView = outputTextureView}};

    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout = bindGroupLayouts[0];
    bindGroupDesc.entryCount = 2;
    bindGroupDesc.entries = bindGroupEntries;
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);

    wgpuComputePassEncoderSetBindGroup(computePass, 0, bindGroup, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroupCountX, workgroupCountY, 1);
    wgpuComputePassEncoderEnd(computePass);

    WGPUCommandBufferDescriptor cmdBufferDesc = {};
    WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);
    wgpuQueueSubmit(*RenderContext::GetQueue(), 1, &commandBuffer);

    wgpuBindGroupRelease(bindGroup);
    wgpuTextureViewRelease(inputTextureView);
    wgpuTextureViewRelease(outputTextureView);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(commandBuffer);
  }

  wgpuPipelineLayoutRelease(pipelineLayout);
  wgpuComputePipelineRelease(pipeline);
}

void Render::ComputeMipCube(TextureCube* input) {
  auto device = RenderContext::GetDevice();
  auto computeShader = ShaderManager::LoadShader(
      "SH_ComputeCube",
      RESOURCE_DIR "/shaders/ComputeMipCube.wgsl");

  // Create pipeline layout
  auto bindGroupLayout = computeShader->GetReflectionInfo().LayoutDescriptors.begin()->second;
  WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
  pipelineLayoutDesc.bindGroupLayoutCount = 1;
  pipelineLayoutDesc.bindGroupLayouts = &bindGroupLayout;
  auto pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

  // Create compute pipeline
  WGPUComputePipelineDescriptor pipelineDesc = {};
  pipelineDesc.layout = pipelineLayout;
  pipelineDesc.compute.entryPoint = MakeLabel("computeMipMap");
  pipelineDesc.compute.module = computeShader->GetNativeShaderModule();
  auto pipeline = wgpuDeviceCreateComputePipeline(device, &pipelineDesc);

  // Common view descriptor template
  WGPUTextureViewDescriptor viewDesc = {
      .format = WGPUTextureFormat_RGBA8Unorm,
      .dimension = WGPUTextureViewDimension_2DArray,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 6  // All faces
  };

  // Process mip chain
  const uint32_t mipCount = RenderUtils::CalculateMipCount(
      input->GetSpec().Width,
      input->GetSpec().Height);
  const uint32_t workgroupSize = 8;

  for (uint32_t mipLevel = 1; mipLevel < mipCount; ++mipLevel) {
    // Calculate dimensions for current mip level
    const uint32_t width = std::max(input->GetSpec().Width >> mipLevel, 1u);
    const uint32_t height = std::max(input->GetSpec().Height >> mipLevel, 1u);
    const uint32_t workgroupsX = (width + workgroupSize - 1) / workgroupSize;
    const uint32_t workgroupsY = (height + workgroupSize - 1) / workgroupSize;

    // Create input view (previous mip level)
    viewDesc.baseMipLevel = mipLevel - 1;
    auto inputView = wgpuTextureCreateView(input->m_TextureBuffer, &viewDesc);

    // Create output view (current mip level)
    viewDesc.baseMipLevel = mipLevel;
    auto outputView = wgpuTextureCreateView(input->m_TextureBuffer, &viewDesc);

    // Create bind group
    WGPUBindGroupEntry bindGroupEntries[2] = {
        {.binding = 0, .textureView = inputView},
        {.binding = 1, .textureView = outputView}};

    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entryCount = 2;
    bindGroupDesc.entries = bindGroupEntries;
    auto bindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);

    // Record and submit commands
    auto encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    {
      auto computePass = wgpuCommandEncoderBeginComputePass(encoder, nullptr);
      wgpuComputePassEncoderSetPipeline(computePass, pipeline);
      wgpuComputePassEncoderSetBindGroup(computePass, 0, bindGroup, 0, nullptr);
      // Dispatch across all faces (6 in Z dimension)
      wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroupsX, workgroupsY, 6);
      wgpuComputePassEncoderEnd(computePass);
    }

    auto commandBuffer = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(*RenderContext::GetQueue(), 1, &commandBuffer);

    // Cleanup per-mip resources
    wgpuBindGroupRelease(bindGroup);
    wgpuTextureViewRelease(inputView);
    wgpuTextureViewRelease(outputView);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(commandBuffer);
  }

  // Cleanup shared resources
  wgpuPipelineLayoutRelease(pipelineLayout);
  wgpuComputePipelineRelease(pipeline);
}

struct PrefilterUniform {
  uint32_t currentMipLevel;
  uint32_t mipLevelCount;
  uint32_t _pad[2];
};

void Render::PreFilter(TextureCube* input) {
  auto device = RenderContext::GetDevice();
  auto computeShader = ShaderManager::LoadShader(
      "SH_ComputeCube2",
      RESOURCE_DIR "/shaders/CubePreFilter.wgsl");

  // Calculate mip parameters
  const uint32_t mipCount = RenderUtils::CalculateMipCount(input->GetSpec().Width, input->GetSpec().Height);
  const uint32_t uniformStride = std::max(
      (uint32_t)sizeof(PrefilterUniform),
      (uint32_t)Instance->m_Limits.minUniformBufferOffsetAlignment);

  // Create uniform buffer for all mip levels
  auto uniformBuffer = GPUAllocator::GAlloc(
      WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
      uniformStride * mipCount);

  // Initialize uniforms for all mip levels
  for (uint32_t mipLevel = 1; mipLevel < mipCount; ++mipLevel) {
    PrefilterUniform uniform = {
        .currentMipLevel = mipLevel,
        .mipLevelCount = mipCount};
    uniformBuffer->SetData(&uniform, mipLevel * uniformStride, sizeof(uniform));
  }

  // Create sampler
  auto sampler = Sampler::Create({.Name = "S_Skybox",
                                  .WrapFormat = TextureWrappingFormat::ClampToEdges,
                                  .MagFilterFormat = FilterMode::Linear,
                                  .MinFilterFormat = FilterMode::Linear,
                                  .MipFilterFormat = FilterMode::Linear});

  // Create pipeline layout
  auto bindGroupLayout = computeShader->GetReflectionInfo().LayoutDescriptors.begin()->second;
  WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
  pipelineLayoutDesc.bindGroupLayoutCount = 1;
  pipelineLayoutDesc.bindGroupLayouts = &bindGroupLayout;
  auto pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

  // Create compute pipeline
  WGPUComputePipelineDescriptor pipelineDesc = {};
  pipelineDesc.layout = pipelineLayout;
  pipelineDesc.compute.entryPoint = MakeLabel("prefilterCubeMap");
  pipelineDesc.compute.module = computeShader->GetNativeShaderModule();
  auto pipeline = wgpuDeviceCreateComputePipeline(device, &pipelineDesc);

  // Common view descriptor template
  WGPUTextureViewDescriptor viewDesc = {
      .format = WGPUTextureFormat_RGBA8Unorm,
      .dimension = WGPUTextureViewDimension_Cube,  // Will be overridden for output
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 6};

  // Process each mip level
  uint32_t width = input->GetSpec().Width;
  uint32_t height = input->GetSpec().Height;
  const uint32_t workgroupSize = 4;

  for (uint32_t mipLevel = 1; mipLevel < mipCount; ++mipLevel) {
    // Update dimensions for current mip level
    width /= 2;
    height /= 2;
    const uint32_t workgroupsX = (width + workgroupSize - 1) / workgroupSize;
    const uint32_t workgroupsY = (height + workgroupSize - 1) / workgroupSize;

    // Create input view (previous mip level)
    viewDesc.dimension = WGPUTextureViewDimension_Cube;
    viewDesc.baseMipLevel = mipLevel - 1;
    auto inputView = wgpuTextureCreateView(input->m_TextureBuffer, &viewDesc);

    // Create output view (current mip level)
    viewDesc.dimension = WGPUTextureViewDimension_2DArray;
    viewDesc.baseMipLevel = mipLevel;
    auto outputView = wgpuTextureCreateView(input->m_TextureBuffer, &viewDesc);

    // Create bind group
    WGPUBindGroupEntry bindGroupEntries[4] = {
        {.binding = 0, .textureView = inputView},
        {.binding = 1, .textureView = outputView},
        {.binding = 2, .sampler = *sampler->GetNativeSampler()},
        {.binding = 3, .buffer = uniformBuffer->Buffer, .size = sizeof(PrefilterUniform)}};

    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entryCount = 4;
    bindGroupDesc.entries = bindGroupEntries;
    auto bindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);

    // Record and submit commands
    auto encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    {
      auto computePass = wgpuCommandEncoderBeginComputePass(encoder, nullptr);
      wgpuComputePassEncoderSetPipeline(computePass, pipeline);

      uint32_t dynamicOffset = mipLevel * uniformStride;
      wgpuComputePassEncoderSetBindGroup(computePass, 0, bindGroup, 1, &dynamicOffset);
      wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroupsX, workgroupsY, 1);
      wgpuComputePassEncoderEnd(computePass);
    }

    auto commandBuffer = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(*RenderContext::GetQueue(), 1, &commandBuffer);

    // Cleanup per-mip resources
    wgpuBindGroupRelease(bindGroup);
    wgpuTextureViewRelease(inputView);
    wgpuTextureViewRelease(outputView);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(commandBuffer);
  }

  // Cleanup shared resources
  wgpuPipelineLayoutRelease(pipelineLayout);
  wgpuComputePipelineRelease(pipeline);
}

void Render::PreFilterAlt(TextureCube* input) {
  auto device = RenderContext::GetDevice();
  auto computeShader = ShaderManager::LoadShader(
      "SH_ComputeCube3",
      RESOURCE_DIR "/shaders/CubePreFilter2.wgsl");

  // Create common texture view descriptor for both input and output
  WGPUTextureViewDescriptor viewDesc = {
      .dimension = WGPUTextureViewDimension_Cube,  // Will be overridden for output
      .baseMipLevel = 0,
      .mipLevelCount = 1,
      .baseArrayLayer = 0,
      .arrayLayerCount = 6};

  // Create input and output views
  viewDesc.format = WGPUTextureFormat_RGBA8Unorm;
  auto inputView = wgpuTextureCreateView(input->m_TextureBuffer, &viewDesc);

  viewDesc.format = WGPUTextureFormat_RGBA32Float;
  viewDesc.dimension = WGPUTextureViewDimension_2DArray;
  auto outputView = wgpuTextureCreateView(input->m_TextureBufferAlt, &viewDesc);

  // Create sampler
  auto sampler = Sampler::Create({.Name = "S_Skybox",
                                  .WrapFormat = TextureWrappingFormat::ClampToEdges,
                                  .MagFilterFormat = FilterMode::Linear,
                                  .MinFilterFormat = FilterMode::Linear,
                                  .MipFilterFormat = FilterMode::Linear});

  // Create pipeline layout
  auto bindGroupLayout = computeShader->GetReflectionInfo().LayoutDescriptors.begin()->second;
  WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
  pipelineLayoutDesc.bindGroupLayoutCount = 1;
  pipelineLayoutDesc.bindGroupLayouts = &bindGroupLayout;
  auto pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

  // Create bind group
  WGPUBindGroupEntry bindGroupEntries[3] = {
      {.binding = 0, .textureView = outputView},
      {.binding = 1, .textureView = inputView},
      {.binding = 2, .sampler = *sampler->GetNativeSampler()}};

  WGPUBindGroupDescriptor bindGroupDesc = {};
  bindGroupDesc.layout = bindGroupLayout;
  bindGroupDesc.entryCount = 3;
  bindGroupDesc.entries = bindGroupEntries;
  auto bindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);

  // Create compute pipeline
  WGPUComputePipelineDescriptor pipelineDesc = {};
  pipelineDesc.layout = pipelineLayout;
  pipelineDesc.compute.module = computeShader->GetNativeShaderModule();
  pipelineDesc.compute.entryPoint = MakeLabel("prefilterCubeMap");
  auto pipeline = wgpuDeviceCreateComputePipeline(device, &pipelineDesc);

  // Set up compute parameters
  const uint32_t workgroupSize = 4;
  const uint32_t width = input->GetSpec().Width;
  const uint32_t height = input->GetSpec().Height;
  const uint32_t workgroupsX = (width + workgroupSize - 1) / workgroupSize;
  const uint32_t workgroupsY = (height + workgroupSize - 1) / workgroupSize;

  // Record and submit commands
  auto encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
  {
    auto computePass = wgpuCommandEncoderBeginComputePass(encoder, nullptr);
    wgpuComputePassEncoderSetPipeline(computePass, pipeline);
    wgpuComputePassEncoderSetBindGroup(computePass, 0, bindGroup, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroupsX, workgroupsY, 1);
    wgpuComputePassEncoderEnd(computePass);
  }

  auto commandBuffer = wgpuCommandEncoderFinish(encoder, nullptr);
  wgpuQueueSubmit(*RenderContext::GetQueue(), 1, &commandBuffer);

  // Cleanup WebGPU resources
  wgpuBindGroupRelease(bindGroup);
  wgpuTextureViewRelease(inputView);
  wgpuTextureViewRelease(outputView);
  wgpuPipelineLayoutRelease(pipelineLayout);
  wgpuComputePipelineRelease(pipeline);
  wgpuCommandEncoderRelease(encoder);
  wgpuCommandBufferRelease(commandBuffer);
}

// WGPUComputePassEncoder BeginComputePass(Ref<RenderPass> pass, WGPUCommandEncoder& encoder) {
//   WGPUComputePassDescriptor computePassDesc = {};
//   WGPUComputePassEncoder computePass = wgpuCommandEncoderBeginComputePass(encoder, &computePassDesc);
//
//   return computePass;
// }

// Queue queue = m_device.getQueue();
//
//	// Initialize a command encoder
//	CommandEncoderDescriptor encoderDesc = Default;
//	CommandEncoder encoder = m_device.createCommandEncoder(encoderDesc);
//
//	// Create compute pass
//	ComputePassDescriptor computePassDesc;
//	computePassDesc.timestampWriteCount = 0;
//	computePassDesc.timestampWrites = nullptr;
//	ComputePassEncoder computePass = encoder.beginComputePass(computePassDesc);
//
//	computePass.setPipeline(m_pipeline);
//
//	for (uint32_t nextLevel = 1; nextLevel < m_textureMipSizes.size(); ++nextLevel) {
//		initBindGroup(nextLevel);
//		computePass.setBindGroup(0, m_bindGroup, 0, nullptr);
//
//		uint32_t invocationCountX = m_textureMipSizes[nextLevel].width;
//		uint32_t invocationCountY = m_textureMipSizes[nextLevel].height;
//		uint32_t workgroupSizePerDim = 8;
//		// This ceils invocationCountX / workgroupSizePerDim
//		uint32_t workgroupCountX = (invocationCountX + workgroupSizePerDim - 1) / workgroupSizePerDim;
//		uint32_t workgroupCountY = (invocationCountY + workgroupSizePerDim - 1) / workgroupSizePerDim;
//		computePass.dispatchWorkgroups(workgroupCountX, workgroupCountY, 1);

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

//#if __EMSCRIPTEN__
  colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
//#endif

  if (renderFrameBuffer->HasColorAttachment()) {
    if (renderFrameBuffer->m_FrameBufferSpec.SwapChainTarget) {
      Instance->m_SwapTexture = Render::Instance->GetCurrentSwapChainTexture();
      colorAttachment.resolveTarget = Instance->m_SwapTexture;
      //colorAttachment.view = Instance->m_SwapTexture;
    }

    colorAttachment.view = renderFrameBuffer->GetAttachment(0)->GetNativeView();
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAttachment;
  }

  if (renderFrameBuffer->HasDepthAttachment()) {
    WGPURenderPassDepthStencilAttachment depthAttachment = {};
    ZERO_INIT(depthAttachment);

    auto depth = renderFrameBuffer->GetDepthAttachment();
    if (depth->m_Views.size() > 1) {
      int layerNum = renderFrameBuffer->m_FrameBufferSpec.ExistingImageLayers[0];
      depthAttachment.view = renderFrameBuffer->GetDepthAttachment()->m_Views[layerNum + 1];
    } else {
      depthAttachment.view = renderFrameBuffer->GetDepthAttachment()->GetNativeView();
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
