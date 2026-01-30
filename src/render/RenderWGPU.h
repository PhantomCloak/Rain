#pragma once
#include "render/Render.h"

namespace Rain
{
  class RenderWGPU : public Render
  {
   public:
    RenderWGPU()
    {
      Render::SetRenderInstance(this);
    };

    ~RenderWGPU()
    {
      Render::SetRenderInstance(nullptr);
    };

    virtual bool Init(void* nativeWindowPointer) override;

    virtual Ref<RenderContext> GetRenderContext() override { return m_RenderContext; }
    virtual Ref<Texture2D> GetWhiteTexture() override;
    virtual Ref<Sampler> GetDefaultSampler() override;

    virtual void BeginRenderPass(Ref<RenderPass> pass, Ref<CommandBuffer> commandBuffer) override;
    virtual void EndRenderPass(Ref<RenderPass> pass) override;

    virtual void RenderMesh(Ref<RenderPass> renderPass,
                            WGPURenderPipeline pipeline,
                            Ref<MeshSource> mesh,
                            uint32_t submeshIndex,
                            Ref<MaterialTable> material,
                            Ref<GPUBuffer> transformBuffer,
                            uint32_t transformOffset,
                            uint32_t instanceCount) override;

    virtual void RenderSkeletalMesh(Ref<RenderPass> renderPass,
                                    WGPURenderPipeline pipeline,
                                    Ref<MeshSource> mesh,
                                    uint32_t submeshIndex,
                                    Ref<MaterialTable> materialTable,
                                    Ref<GPUBuffer> transformBuffer,
                                    uint32_t instanceCount) override;

    virtual void SubmitFullscreenQuad(Ref<RenderPass> renderPass, WGPURenderPipeline pipeline) override;

    virtual GLFWwindow* GetActiveWindow() override { return m_Window; }

    virtual Ref<CommandEncoder> CreateCommandEncoder() override;
    virtual void SubmitAndFinishEncoder(Ref<CommandEncoder> commandEncoder) override;

    virtual void ComputeMip(Texture2D* output) override;
    virtual void ComputeMipCube(TextureCube* output) override;
    virtual void ComputePreFilter(TextureCube* input, TextureCube* output) override;
    virtual void ComputeEnvironmentIrradiance(TextureCube* input, TextureCube* output) override;
    virtual void ComputeEquirectToCubemap(Texture2D* equirectTexture, TextureCube* outputCubemap) override;
    virtual std::pair<Ref<TextureCube>, Ref<TextureCube>> CreateEnvironmentMap(const std::string& filepath) override;

    virtual bool IsReady() override { return Instance && Instance->m_DawnInstance && Instance->m_Adapter && Instance->m_Device; }
    virtual void Tick() override;

   private:
    void Initialize();
    void StartRequestInstance(void* nativeWindowPtr);
    void StartRequestDevice();
    WGPUTextureView GetCurrentTextureView();

#ifndef __EMSCRIPTEN__
    static void OnDeviceCallback(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* userdata2);
    static void OnAdapterInstanceCallback(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2);
#else
    static void OnDeviceCallback(WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* userdata1);
#endif

    static RenderWGPU* Instance;

    Ref<RenderContext> m_RenderContext;
    WGPUInstance m_DawnInstance = nullptr;
    WGPUDevice m_Device = nullptr;
    WGPUAdapter m_Adapter = nullptr;
    WGPUQueue m_Queue = nullptr;

    GLFWwindow* m_Window = nullptr;
    WGPULimits m_Limits;

    WGPUTextureFormat m_swapChainFormat = WGPUTextureFormat_Undefined;
    WGPUTextureFormat m_depthTextureFormat = WGPUTextureFormat_Depth24Plus;

    WGPUColor m_ClearColor = WGPUColor{0, 0, 0, 1};

    friend class RenderContext;
  };
}  // namespace Rain
