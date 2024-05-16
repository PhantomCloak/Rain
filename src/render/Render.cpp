#include "Render.h"
#include "Mesh.h"
#include <map>

#if __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <iostream>

Render* Render::Instance = nullptr;

WGPUInstance Render::CreateInstance() {
  WGPUInstanceDescriptor desc = {};
  desc.nextInChain = nullptr;

  WGPUInstance instance = wgpuCreateInstance(&desc);
  if (!instance) {
    std::cerr << "Could not initialize WebGPU!" << std::endl;
    assert(true);
  }
  return instance;
}

bool Render::Init(void* window, WGPUInstance instance, WGPUSurface surface) {
	Instance = this;

  WGPURequestAdapterOptions adapterOpts{};
  adapterOpts.compatibleSurface = surface;
  m_adapter = requestAdapter(instance, &adapterOpts);

  WGPURequiredLimits requiredLimits = GetRequiredLimits(m_adapter);
  static WGPUDeviceDescriptor deviceDesc;

  deviceDesc.label = "My Device";
#if __EMSCRIPTEN__
  deviceDesc.requiredFeaturesCount = 0;
#else
  deviceDesc.requiredFeatureCount = 0;
#endif
  deviceDesc.requiredLimits = &requiredLimits;
  deviceDesc.defaultQueue.label = "The default queue";

  m_device = requestDevice(m_adapter, &deviceDesc);

  std::cout << "Got device: " << m_device << std::endl;

  // Hack

  m_queue = wgpuDeviceGetQueue(m_device);

  int width, height;
  glfwGetFramebufferSize((GLFWwindow*)window, &width, &height);
	std::cout << "FBO SIZE! (w,h): " << width << ", " << height << std::endl;

#if __EMSCRIPTEN__
  WGPUTextureFormat swapChainFormat = wgpuSurfaceGetPreferredFormat(surface, m_adapter);
#else
  WGPUTextureFormat swapChainFormat = WGPUTextureFormat_BGRA8Unorm;
#endif

  m_swapChainFormat = swapChainFormat;

  m_swapChainDesc = {};  // Zero-initialize the struct

  m_swapChainDesc.width = (uint32_t)width;
  m_swapChainDesc.height = (uint32_t)height;
  m_swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
  m_swapChainDesc.format = swapChainFormat;
  m_swapChainDesc.presentMode = WGPUPresentMode_Fifo;

  if (m_swapChain != NULL) {
    wgpuSwapChainRelease(m_swapChain);
  }

  m_swapChain = buildSwapChain(m_swapChainDesc, m_device, surface);
	m_depthTexture = GetDepthBufferTexture(m_device,m_depthTextureFormat, m_swapChainDesc.width, m_swapChainDesc.height);
  m_depthTextureView = GetDepthBufferTextureView("T_Depth_Default", m_depthTexture, m_depthTextureFormat);

  m_sampler = AttachSampler(m_device);

	wgpuDeviceSetUncapturedErrorCallback(m_device, [](WGPUErrorType errorType, const char* message, void* userdata) {
    fprintf(stderr, "Dawn error: %s\n", message);
		exit(0);
}, nullptr);

  return true;
}

WGPUAdapter Render::requestAdapter(WGPUInstance instance,
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
    if (status == WGPURequestAdapterStatus_Success) {
      userData.adapter = adapter;
    } else {
      std::cout << "Could not get WebGPU adapter: " << message << std::endl;
    }
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

WGPUDevice Render::requestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor) {
  struct UserData {
    WGPUDevice device = nullptr;
    bool requestEnded = false;
  };
  UserData userData;

  auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status,
                                 WGPUDevice device, char const* message,
                                 void* pUserData) {
    UserData& userData = *reinterpret_cast<UserData*>(pUserData);
    if (status == WGPURequestDeviceStatus_Success) {
      userData.device = device;
    } else {
      std::cout << "Could not get WebGPU device: " << message << std::endl;
    }
    userData.requestEnded = true;
  };

  wgpuAdapterRequestDevice(adapter, descriptor, onDeviceRequestEnded,
                           (void*)&userData);

  // saa
#if __EMSCRIPTEN__
  while (!userData.requestEnded) {
    emscripten_sleep(100);
  }
#endif

  assert(userData.requestEnded);

  return userData.device;
}

WGPURequiredLimits Render::GetRequiredLimits(WGPUAdapter adapter) {
  WGPUSupportedLimits supportedLimits = {};  // Zero-initialize the struct

#ifdef __EMSCRIPTEN__
                                             // Error in Chrome handling
  supportedLimits.limits.minStorageBufferOffsetAlignment = 256;
  supportedLimits.limits.minUniformBufferOffsetAlignment = 256;
#else
  wgpuAdapterGetLimits(adapter, &supportedLimits);
#endif

  static WGPURequiredLimits requiredLimits = {};  // Assuming this is how the default is defined
  requiredLimits.limits.maxVertexAttributes = 4;
  requiredLimits.limits.maxVertexBuffers = 8;
  requiredLimits.limits.maxBufferSize = 150000 * sizeof(VertexAttributes);
  requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes);
  requiredLimits.limits.minStorageBufferOffsetAlignment =
      supportedLimits.limits.minStorageBufferOffsetAlignment;
  requiredLimits.limits.minUniformBufferOffsetAlignment =
      supportedLimits.limits.minUniformBufferOffsetAlignment;
  requiredLimits.limits.maxInterStageShaderComponents = 8;
  requiredLimits.limits.maxBindGroups = 2;
  requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
  requiredLimits.limits.maxUniformBufferBindingSize = 32 * 4 * sizeof(float);
  requiredLimits.limits.maxTextureDimension1D = 2048;
  requiredLimits.limits.maxTextureDimension2D = 2048;
  requiredLimits.limits.maxTextureArrayLayers = 1;
  requiredLimits.limits.maxSampledTexturesPerShaderStage = 1;
  requiredLimits.limits.maxSamplersPerShaderStage = 1;

  return requiredLimits;
}

WGPUSwapChainDescriptor Render::GetSwapchainDescriptor(int width, int height, WGPUTextureFormat swapChainFormat) {
}

WGPUSwapChain Render::buildSwapChain(WGPUSwapChainDescriptor descriptor, WGPUDevice device, WGPUSurface surface) {
  if (m_swapChain != NULL) {
    wgpuSwapChainRelease(m_swapChain);
  }

  return wgpuDeviceCreateSwapChain(m_device, surface, &m_swapChainDesc);
}

WGPUTexture Render::GetDepthBufferTexture(WGPUDevice device, WGPUTextureFormat format, int width, int height, bool dbg) {
  // Destroy previously allocated texture
  //if (m_depthTexture != NULL) {
  //  wgpuTextureViewRelease(m_depthTextureView);
  //  wgpuTextureDestroy(m_depthTexture);
  //  wgpuTextureRelease(m_depthTexture);
  //}

  // Create the depth texture
  WGPUTextureDescriptor depthTextureDesc = {};
  depthTextureDesc.dimension = WGPUTextureDimension_2D;
  depthTextureDesc.format = format;
  depthTextureDesc.mipLevelCount = 1;
  depthTextureDesc.sampleCount = 1;
  depthTextureDesc.size.width = width;
  depthTextureDesc.size.height = height;
  depthTextureDesc.size.depthOrArrayLayers = 1;

	if(dbg)
		depthTextureDesc.usage = WGPUTextureUsage_RenderAttachment |  WGPUTextureUsage_TextureBinding;
	else
		depthTextureDesc.usage = WGPUTextureUsage_RenderAttachment;

  depthTextureDesc.viewFormatCount = 1;
  depthTextureDesc.viewFormats = &m_depthTextureFormat;

  WGPUTexture depthTexture = wgpuDeviceCreateTexture(device, &depthTextureDesc);

  printf("Depth texture: %p\n", depthTexture);
  return depthTexture;
}

WGPUTextureView Render::GetDepthBufferTextureView(std::string label, WGPUTexture& depthTexture, WGPUTextureFormat depthTextureFormat) {

	static std::map<std::string, WGPUTextureViewDescriptor> depthTextureDesc;

	if(depthTextureDesc.find(label) == depthTextureDesc.end())
	{
		depthTextureDesc[label] = {};
	}

  WGPUTextureViewDescriptor& depthTextureViewDesc = depthTextureDesc[label];
	depthTextureViewDesc.label = label.c_str();
  depthTextureViewDesc.aspect = WGPUTextureAspect_DepthOnly;
  depthTextureViewDesc.baseArrayLayer = 0;
  depthTextureViewDesc.arrayLayerCount = 1;
  depthTextureViewDesc.baseMipLevel = 0;
  depthTextureViewDesc.mipLevelCount = 1;
  depthTextureViewDesc.dimension = WGPUTextureViewDimension_2D;
  depthTextureViewDesc.format = depthTextureFormat;

  WGPUTextureView depthTextureView =
      wgpuTextureCreateView(depthTexture, &depthTextureViewDesc);

  std::cout << "Depth texture view: " << depthTextureView << std::endl;

  return depthTextureView;
}


WGPUSampler Render::AttachSampler(WGPUDevice device) {
  WGPUSamplerDescriptor samplerDesc = {};  // Zero-initialize the struct

  // Set the properties
  samplerDesc.addressModeU = WGPUAddressMode_Repeat;
  samplerDesc.addressModeV = WGPUAddressMode_Repeat;
  samplerDesc.addressModeW = WGPUAddressMode_Repeat;
  samplerDesc.magFilter = WGPUFilterMode_Linear;
  samplerDesc.minFilter = WGPUFilterMode_Linear;
  samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
  samplerDesc.lodMinClamp = 0.0f;
  samplerDesc.lodMaxClamp = 80.0f;
  samplerDesc.compare = WGPUCompareFunction_Undefined;
  samplerDesc.maxAnisotropy = 1;

  return wgpuDeviceCreateSampler(device, &samplerDesc);
}
