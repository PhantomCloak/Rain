#pragma once
#include <webgpu/webgpu.h>
#include <unordered_map>
#include "GLFW/glfw3.h"
#include "Material.h"
#include "render/Mesh.h"
#include "render/RenderPass.h"

class Render {
 public:
  static Render* Instance;
  bool Init(void* window);

  WGPUSwapChain BuildSwapChain(WGPUSwapChainDescriptor descriptor, WGPUDevice device, WGPUSurface surface);
  Ref<RenderContext> GetRenderContext() { return m_RenderContext; }

  static WGPURenderPassEncoder BeginRenderPass(Ref<RenderPass> pass, WGPUCommandEncoder& encoder);
  static void EndRenderPass(Ref<RenderPass> pass, WGPURenderPassEncoder& encoder);

  static void RenderMesh(WGPURenderPassEncoder& renderCommandBuffer,
                  WGPURenderPipeline pipeline,
                  Ref<MeshSource> mesh,
                  uint32_t submeshIndex,
                  Ref<Material> material,
                  Ref<GPUBuffer> transformBuffer,
                  uint32_t transformOffset,
                  uint32_t instanceCount);
  static void SubmitFullscreenQuad(WGPURenderPassEncoder& renderCommandBuffer, WGPURenderPipeline pipeline);

  std::unordered_map<UUID, Material> Materials;

  Ref<Texture> GetCurrentSwapChainTexture();

  WGPUSurface GetActiveSurface() { return m_surface; }

 private:
  void RendererPostInit();
  WGPUInstance CreateGPUInstance();
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
  WGPUSwapChain m_swapChain = nullptr;
  WGPUTextureFormat m_swapChainFormat = WGPUTextureFormat_Undefined;
  WGPUTextureFormat m_depthTextureFormat = WGPUTextureFormat_Depth24Plus;

  WGPUSwapChainDescriptor m_swapChainDesc;

  friend class RenderContext;
};
