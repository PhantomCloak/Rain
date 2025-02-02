#pragma once
#include <webgpu/webgpu.h>
#include <unordered_map>
#include "GLFW/glfw3.h"
#include "Material.h"
#include "render/Mesh.h"
#include "render/Pipeline.h"
#include "render/RenderPass.h"

class Render {
 public:
  static Render* Instance;
  bool Init(void* window);

  Ref<RenderContext> GetRenderContext() { return m_RenderContext; }

  static Ref<Texture2D> GetWhiteTexture();
  static Ref<Sampler> GetDefaultSampler();

	void ConfigureSurface(uint32_t width, uint32_t height);

  static WGPURenderPassEncoder BeginRenderPass(Ref<RenderPass> pass, WGPUCommandEncoder& encoder);
  // static WGPUComputePassEncoder BeginComputePass(Ref<RenderPass> pass, WGPUCommandEncoder& encoder);

  static void EndRenderPass(Ref<RenderPass> pass, WGPURenderPassEncoder& encoder);

  static void RenderMesh(WGPURenderPassEncoder& renderCommandBuffer,
                         WGPURenderPipeline pipeline,
                         Ref<MeshSource> mesh,
                         uint32_t submeshIndex,
                         Ref<MaterialTable> material,
                         Ref<GPUBuffer> transformBuffer,
                         uint32_t transformOffset,
                         uint32_t instanceCount);
  static void SubmitFullscreenQuad(WGPURenderPassEncoder& renderCommandBuffer, WGPURenderPipeline pipeline);

  std::unordered_map<UUID, Material> Materials;

  WGPUTextureView GetCurrentSwapChainTexture();

  WGPUSurface GetActiveSurface() { return m_surface; }

  static void ComputeMip(Texture2D* output);
  static void ComputeMipCube(TextureCube* output);
  static void PreFilter(TextureCube* output);
  static void PreFilterAlt(TextureCube* output);
  static bool saveTexture(const std::filesystem::path path, WGPUDevice device, Ref<Texture2D> texture, int mipLevel);

  static void RegisterShaderDependency(Ref<Shader> shader, Material* material);
  static void RegisterShaderDependency(Ref<Shader> shader, RenderPipeline* material);
  static void ReloadShaders();
  static void ReloadShader(Ref<Shader> shader);

 private:
  void RendererPostInit();
  WGPUInstance CreateGPUInstance();
  WGPURequiredLimits* GetRequiredLimits(WGPUAdapter adapter);
  WGPUAdapter RequestAdapter(WGPUInstance instance, WGPURequestAdapterOptions const* options);
  WGPUDevice RequestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor);

	WGPUTextureView GetCurrentTextureView();

 public:
  Ref<RenderContext> m_RenderContext;
  Ref<RenderPass> m_CurrentRenderPass;
  WGPUSurface m_surface = nullptr;
  GLFWwindow* m_window = nullptr;
  WGPUAdapter m_adapter = nullptr;
  WGPUDevice m_device = nullptr;
  WGPUQueue m_queue = nullptr;
  WGPUInstance m_Instance = nullptr;
  WGPUTextureView m_SwapTexture;
	WGPULimits m_Limits;

  WGPUTextureFormat m_swapChainFormat = WGPUTextureFormat_Undefined;
  WGPUTextureFormat m_depthTextureFormat = WGPUTextureFormat_Depth24Plus;

  friend class RenderContext;
};
