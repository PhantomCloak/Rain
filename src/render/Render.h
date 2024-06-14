#pragma once
#include <webgpu/webgpu.h>
#include <unordered_map>
#include "GLFW/glfw3.h"
#include "Material.h"
#include "render/Mesh.h"

class Render {
 public:
  static Render* Instance;
  WGPUInstance CreateInstance();
  bool Init(void* window, WGPUInstance instance);
  void OnFrameStart();
  void OnFrameEnd();

  WGPUSwapChain BuildSwapChain(WGPUSwapChainDescriptor descriptor, WGPUDevice device, WGPUSurface surface);
  WGPUTexture GetDepthBufferTexture(WGPUDevice device, WGPUTextureFormat format, int width, int height, bool dbg = false);
  WGPUTextureView GetDepthBufferTextureView(std::string label, WGPUTexture& depthTexture, WGPUTextureFormat depthTextureFormat);

  Ref<RenderContext> GetRenderContext() { return m_RenderContext; }

  void RenderMesh(WGPURenderPassEncoder& renderCommandBuffer,
                  WGPURenderPipeline pipeline,
                  Ref<MeshSource> mesh,
                  uint32_t submeshIndex,
                  Ref<Material> material,
                  Ref<GPUBuffer> transformBuffer,
                  uint32_t transformOffset,
                  uint32_t instanceCount);

  std::unordered_map<UUID, Material> Materials;

 private:
  WGPURequiredLimits GetRequiredLimits(WGPUAdapter adapter);
  WGPUAdapter RequestAdapter(WGPUInstance instance, WGPURequestAdapterOptions const* options);
  WGPUDevice RequestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor);

 public:
  Ref<RenderContext> m_RenderContext;
  WGPUSurface m_surface = nullptr;
  GLFWwindow* m_window = nullptr;
  WGPUAdapter m_adapter = nullptr;
  WGPUDevice m_device = nullptr;
  WGPUQueue m_queue = nullptr;
  WGPUTexture m_depthTexture = nullptr;
  WGPUTextureView m_depthTextureView = nullptr;
  WGPUSwapChain m_swapChain = nullptr;
  WGPUSampler m_sampler = nullptr;
  WGPUTextureFormat m_swapChainFormat = WGPUTextureFormat_Undefined;
  WGPUTextureFormat m_depthTextureFormat = WGPUTextureFormat_Depth24Plus;

  WGPUSwapChainDescriptor m_swapChainDesc;

	friend class RenderContext;
};
