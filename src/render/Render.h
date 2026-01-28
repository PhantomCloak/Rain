#pragma once
#include <webgpu/webgpu.h>
#if __EMSCRIPTEN__
using WGPUStringView = const char*;
#endif
#include "GLFW/glfw3.h"
#include "Material.h"
#include "render/Mesh.h"
#include "render/Pipeline.h"
#include "render/RenderPass.h"
#include "render/CommandBuffer.h"

namespace Rain
{
  using OnRendererReadyCallback = std::function<void()>;
  class RenderPassEncoder;
  class CommandEncoder;

  class Render
  {
   public:
    Render() = default;
    ~Render() = default;

    virtual bool Init(void* nativeWindowPointer) = 0;
    static Render* Get() { return m_RenderInstance; }
    static void SetRenderInstance(Render* instance) { m_RenderInstance = instance; }

    virtual Ref<RenderContext> GetRenderContext() = 0;

    virtual Ref<Texture2D> GetWhiteTexture() = 0;
    virtual Ref<Sampler> GetDefaultSampler() = 0;

    virtual void BeginRenderPass(Ref<RenderPass> pass, Ref<CommandBuffer> encoder) = 0;
    virtual void EndRenderPass(Ref<RenderPass> pass) = 0;

    virtual void RenderMesh(Ref<RenderPass> renderCommandBuffer,
                            WGPURenderPipeline pipeline,
                            Ref<MeshSource> mesh,
                            uint32_t submeshIndex,
                            Ref<MaterialTable> material,
                            Ref<GPUBuffer> transformBuffer,
                            uint32_t transformOffset,
                            uint32_t instanceCount) = 0;

    virtual void RenderSkeletalMesh(Ref<RenderPass> renderCommandBuffer,
                                    WGPURenderPipeline pipeline,
                                    Ref<MeshSource> mesh,
                                    uint32_t submeshIndex,
                                    Ref<MaterialTable> materialTable,
                                    Ref<GPUBuffer> transformBuffer,
                                    uint32_t instanceCount) = 0;

    virtual void SubmitFullscreenQuad(Ref<RenderPass> renderCommandBuffer, WGPURenderPipeline pipeline) = 0;

    virtual GLFWwindow* GetActiveWindow() = 0;

    virtual Ref<CommandEncoder> CreateCommandEncoder() = 0;
    virtual void SubmitAndFinishEncoder(Ref<CommandEncoder> commandEncoder) = 0;

    virtual void ComputeMip(Texture2D* output) = 0;
    virtual void ComputeMipCube(TextureCube* output) = 0;
    virtual void ComputePreFilter(TextureCube* input, TextureCube* output) = 0;
    virtual void ComputeEnvironmentIrradiance(TextureCube* input, TextureCube* output) = 0;
    virtual void ComputeEquirectToCubemap(Texture2D* equirectTexture, TextureCube* outputCubemap) = 0;
    virtual std::pair<Ref<TextureCube>, Ref<TextureCube>> CreateEnvironmentMap(const std::string& filepath) = 0;

    static void RegisterShaderDependency(Ref<Shader> shader, Material* material);
    static void RegisterShaderDependency(Ref<Shader> shader, RenderPipeline* material);
    static void ReloadShaders();
    static void ReloadShader(Ref<Shader> shader);

    virtual bool IsReady() = 0;
    virtual void Tick() = 0;

    OnRendererReadyCallback OnReady;

   private:
    static Render* m_RenderInstance;
    GLFWwindow* m_Window = nullptr;
    static void OnDeviceCallback(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* userdata2);
    static void OnAdapterInstanceCallback(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2);
    void RendererPostInit();
  };
}  // namespace Rain
