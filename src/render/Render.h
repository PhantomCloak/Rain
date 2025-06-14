#pragma once
#include <webgpu/webgpu.h>
#include "GLFW/glfw3.h"
#include "Material.h"
#include "render/Mesh.h"
#include "render/Pipeline.h"
#include "render/RenderPass.h"

namespace Rain {
  class Render {
   public:
    static Render* Instance;
    bool Init(void* window);

    Ref<RenderContext> GetRenderContext() { return m_RenderContext; }

    static Ref<Texture2D> GetWhiteTexture();
    static Ref<Sampler> GetDefaultSampler();

    static WGPURenderPassEncoder BeginRenderPass(Ref<RenderPass> pass, WGPUCommandEncoder& encoder);
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

    WGPUTextureView GetCurrentSwapChainTexture();
    WGPUSurface GetActiveSurface() { return m_Surface; }

    static void ComputeMip(Texture2D* output);
    static void ComputeMipCube(TextureCube* output);
    static void ComputePreFilter(TextureCube* input, TextureCube* output);
    static void ComputeEnvironmentIrradiance(TextureCube* input, TextureCube* output);
    static void ComputeEquirectToCubemap(Texture2D* equirectTexture, TextureCube* outputCubemap);
    static std::pair<Ref<TextureCube>, Ref<TextureCube>> CreateEnvironmentMap(const std::string& filepath);

    static void RegisterShaderDependency(Ref<Shader> shader, Material* material);
    static void RegisterShaderDependency(Ref<Shader> shader, RenderPipeline* material);
    static void ReloadShaders();
    static void ReloadShader(Ref<Shader> shader);

   private:
    void RendererPostInit();
    WGPURequiredLimits* GetRequiredLimits(WGPUAdapter adapter);
    WGPUTextureView GetCurrentTextureView();

   public:
    Ref<RenderContext> m_RenderContext;
    WGPUSurface m_Surface = nullptr;
    GLFWwindow* m_Window = nullptr;
    WGPUDevice m_Device = nullptr;
    WGPUQueue m_Queue = nullptr;
    WGPUTextureView m_SwapTexture;
    WGPULimits m_Limits;

    WGPUTextureFormat m_swapChainFormat = WGPUTextureFormat_Undefined;
    WGPUTextureFormat m_depthTextureFormat = WGPUTextureFormat_Depth24Plus;

    WGPUColor m_ClearColor = WGPUColor{0, 0, 0, 1};

    friend class RenderContext;
  };
}  // namespace Rain
