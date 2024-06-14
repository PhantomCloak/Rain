#include "Render.h"
#include <map>
#include "Mesh.h"
#include "core/Assert.h"

#if __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <dawn/dawn_proc.h>
#endif
#include <iostream>

Render* Render::Instance = nullptr;

WGPUInstance Render::CreateInstance() {
  WGPUInstanceDescriptor instanceDesc;
	static WGPUInstance instance;

#if !__EMSCRIPTEN__
  static std::vector<const char*> enabledToggles = {
      "allow_unsafe_apis",
  };

  WGPUDawnTogglesDescriptor dawnToggleDesc;
  dawnToggleDesc.chain.next = nullptr;
  dawnToggleDesc.chain.sType = WGPUSType_DawnTogglesDescriptor;

  dawnToggleDesc.enabledToggles = enabledToggles.data();
  dawnToggleDesc.enabledToggleCount = 1;
  dawnToggleDesc.disabledToggleCount = 0;

  instanceDesc.nextInChain = &dawnToggleDesc.chain;

  instanceDesc.features.timedWaitAnyEnable = 0;
  instanceDesc.features.timedWaitAnyMaxCount = 64;
#endif

  instance = wgpuCreateInstance(&instanceDesc);

	RN_CORE_ASSERT(instance, "An error occured while acquiring the WebGPU instance.");

	return instance;
}

bool Render::Init(void* window, WGPUInstance instance) {
  Instance = this;

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

	m_RenderContext = CreateRef<RenderContext>(m_device, m_queue);

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
  m_depthTexture = GetDepthBufferTexture(m_device, m_depthTextureFormat, m_swapChainDesc.width, m_swapChainDesc.height);
  m_depthTextureView = GetDepthBufferTextureView("T_Depth_Default", m_depthTexture, m_depthTextureFormat);

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

  m_sampler = wgpuDeviceCreateSampler(m_device, &samplerDesc);

  wgpuDeviceSetUncapturedErrorCallback(
      m_device, [](WGPUErrorType errorType, const char* message, void* userdata) {
        fprintf(stderr, "Dawn error: %s\n", message);
        exit(0);
      },
      nullptr);

  return true;
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
  static WGPUSupportedLimits supportedLimits = {};  // Zero-initialize the struct

#ifdef __EMSCRIPTEN__
                                                    // Error in Chrome handling
  supportedLimits.limits.minStorageBufferOffsetAlignment = 256;
  supportedLimits.limits.minUniformBufferOffsetAlignment = 256;
#else
  wgpuAdapterGetLimits(adapter, &supportedLimits);
#endif

  static WGPURequiredLimits requiredLimits = {};  // Assuming this is how the default is defined
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

WGPUTexture Render::GetDepthBufferTexture(WGPUDevice device, WGPUTextureFormat format, int width, int height, bool dbg) {
  // Create the depth texture
  WGPUTextureDescriptor depthTextureDesc = {};
  depthTextureDesc.dimension = WGPUTextureDimension_2D;
  depthTextureDesc.format = format;
  depthTextureDesc.mipLevelCount = 1;
  depthTextureDesc.sampleCount = 1;
  depthTextureDesc.size.width = width;
  depthTextureDesc.size.height = height;
  depthTextureDesc.size.depthOrArrayLayers = 1;

  if (dbg) {
    depthTextureDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
  } else {
    depthTextureDesc.usage = WGPUTextureUsage_RenderAttachment;
  }

  depthTextureDesc.viewFormatCount = 1;
  depthTextureDesc.viewFormats = &m_depthTextureFormat;

  WGPUTexture depthTexture = wgpuDeviceCreateTexture(device, &depthTextureDesc);

  return depthTexture;
}

WGPUTextureView Render::GetDepthBufferTextureView(std::string label, WGPUTexture& depthTexture, WGPUTextureFormat depthTextureFormat) {
  static std::map<std::string, WGPUTextureViewDescriptor> depthTextureDesc;

  if (depthTextureDesc.find(label) == depthTextureDesc.end()) {
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

void Render::RenderMesh(WGPURenderPassEncoder& renderCommandBuffer,
                        WGPURenderPipeline pipeline,
                        Ref<MeshSource> mesh,
                        uint32_t submeshIndex,
                        Ref<Material> material,
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
  if (material) {
    wgpuRenderPassEncoderSetBindGroup(renderCommandBuffer, 1, material->bgMaterial, 0, 0);
  }

  auto& subMesh = mesh->m_SubMeshes[submeshIndex];
  wgpuRenderPassEncoderDrawIndexed(renderCommandBuffer, subMesh.IndexCount, instanceCount, subMesh.BaseIndex, subMesh.BaseVertex, 0);
}
