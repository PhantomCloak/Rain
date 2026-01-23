#include "RenderWGPU.h"
#include <memory>
#include "Mesh.h"
#include "ResourceManager.h"
#include "core/Assert.h"
#include "core/Log.h"
#include "core/Ref.h"
#include "debug/Profiler.h"
#include "render/ShaderManager.h"
#include "webgpu/webgpu.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#if __EMSCRIPTEN__
#include <emscripten.h>
#include "platform/web/web_window.h"
#else
#include "dawn/native/DawnNative.h"
#include <dawn/dawn_proc.h>
#include <GLFW/glfw3native.h>
#include "webgpu/webgpu_glfw.h"
#endif
#include "render/CommandEncoder.h"

namespace Rain
{
  RenderWGPU* RenderWGPU::Instance = nullptr;

  struct WGPURendererData
  {
    Ref<GPUBuffer> QuadVertexBuffer;
    Ref<GPUBuffer> QuadIndexBuffer;
  };

  struct ShaderDependencies
  {
    std::vector<RenderPipeline*> Pipelines;
    std::vector<Material*> Materials;
  };

  static std::map<std::string, ShaderDependencies> s_ShaderDependencies;

  static WGPURendererData* s_Data = nullptr;

  struct ComputeDispatchInfo
  {
    uint32_t workgroupsX;
    uint32_t workgroupsY;
    uint32_t workgroupsZ;
    uint32_t workgroupSize;
  };

  inline ComputeDispatchInfo CalculateDispatchInfo(
      uint32_t width,
      uint32_t height,
      uint32_t depth = 1,
      uint32_t workgroupSize = 32)
  {
    ComputeDispatchInfo info;
    info.workgroupSize = workgroupSize;
    info.workgroupsX = (width + workgroupSize - 1) / workgroupSize;
    info.workgroupsY = (height + workgroupSize - 1) / workgroupSize;
    info.workgroupsZ = depth;

    return info;
  }

  bool RenderWGPU::Init(void* nativeWindowPtr)
  {
    if (!nativeWindowPtr)
    {
      RN_LOG_ERR("Native window pointer is null!");
      return false;
    }

    Instance = this;
    StartRequestInstance(nativeWindowPtr);

    return true;
  }

  // WGPULimits* RenderWGPU::GetRequiredLimits(WGPUAdapter adapter) {
  //   static WGPUSupportedLimits supportedLimits = {};
  //   supportedLimits.nextInChain = nullptr;

  // #ifdef __EMSCRIPTEN__
  //    // Specific limits required for Emscripten
  //    supportedLimits.limits.minStorageBufferOffsetAlignment = 256;
  //    supportedLimits.limits.minUniformBufferOffsetAlignment = 256;
  // #else
  //    wgpuAdapterGetLimits(adapter, &supportedLimits);
  // #endif

  //  WGPURequiredLimits* requiredLimits = (WGPURequiredLimits*)malloc(sizeof(WGPURequiredLimits));
  //  requiredLimits->limits = supportedLimits.limits;  // Copy all supported limits
  //  m_Limits = supportedLimits.limits;

  //  // Override specific limits as needed
  //  requiredLimits->limits.maxBufferSize = 150000 * sizeof(VertexAttribute);
  //  requiredLimits->limits.maxVertexBufferArrayStride = sizeof(VertexAttribute);  // Ensure only one assignment
  //  requiredLimits->limits.maxTextureDimension1D = 4096;
  //  requiredLimits->limits.maxTextureDimension2D = 4096;

  // #ifndef __EMSCRIPTEN__
  //    requiredLimits->limits.maxTextureDimension1D = 6098;
  //    requiredLimits->limits.maxTextureDimension2D = 6098;
  // #endif
  //    // Add any additional specific overrides below

  //  RN_LOG("Max Texture Limit: {}", supportedLimits.limits.maxTextureDimension2D);
  //  requiredLimits->nextInChain = nullptr;
  //  return requiredLimits;
  //}

  WGPUTextureView RenderWGPU::GetCurrentTextureView()
  {
    WGPUSurfaceTexture surfaceTexture = {};
    wgpuSurfaceGetCurrentTexture(m_Surface, &surfaceTexture);

#ifndef __EMSCRIPTEN__
    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal)
#else
    if (surfaceTexture.status != 0)
#endif
    {
      switch (surfaceTexture.status)
      {
        case WGPUSurfaceGetCurrentTextureStatus_Lost:
          // Reconfigure surface here
          // ConfigureSurface(m_swapChainDesc.width, m_swapChainDesc.height);
          wgpuSurfaceGetCurrentTexture(m_Surface, &surfaceTexture);
          break;
#ifndef __EMSCRIPTEN__
        case WGPUSurfaceGetCurrentTextureStatus_Error:
          RN_CORE_ASSERT(false, "Out of memory when acquiring next swapchain texture");
          return nullptr;
#else
        case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
          RN_CORE_ASSERT(false, "Out of memory when acquiring next swapchain texture");
          return nullptr;
#endif
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

  Ref<Texture2D> RenderWGPU::GetWhiteTexture()
  {
    static auto whiteTexture = Rain::ResourceManager::GetTexture("T_Default");
    RN_ASSERT(whiteTexture->GetReadableView() != 0, "Material: Default texture couldn't found.");
    return whiteTexture;
  }

  Ref<Sampler> RenderWGPU::GetDefaultSampler()
  {
    static auto sampler = Sampler::Create(Sampler::GetDefaultProps("S_DefaultSampler"));
    RN_ASSERT(sampler != 0, "Material: Default sampler couldn't found.");
    return sampler;
  }

  void RenderWGPU::ComputeMip(Texture2D* input)
  {
    auto dir = RESOURCE_DIR "/shaders/ComputeMip.wgsl";
    static Ref<Shader> computeShader = ShaderManager::LoadShader("SH_Compute", dir);
    auto device = RenderContext::GetDevice();

    std::vector<WGPUBindGroupLayout> bindGroupLayouts;
    for (const auto& [_, layout] : computeShader->GetReflectionInfo().LayoutDescriptors)
    {
      bindGroupLayouts.push_back(layout);
    }

    WGPUPipelineLayoutDescriptor layoutDesc = {};
    layoutDesc.bindGroupLayoutCount = bindGroupLayouts.size();
    layoutDesc.bindGroupLayouts = bindGroupLayouts.data();
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &layoutDesc);

    WGPUComputePipelineDescriptor computePipelineDesc = {};
    computePipelineDesc.compute.constantCount = 0;
    computePipelineDesc.compute.constants = nullptr;
    computePipelineDesc.compute.entryPoint = RenderUtils::MakeLabel("computeMipMap");
    computePipelineDesc.compute.module = computeShader->GetNativeShaderModule();
    computePipelineDesc.layout = pipelineLayout;
    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &computePipelineDesc);

    int mipCount = RenderUtils::CalculateMipCount(input->GetSpec().Width, input->GetSpec().Height);

    for (uint32_t mipLevel = 1; mipLevel < mipCount; ++mipLevel)
    {
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

  void RenderWGPU::ComputeMipCube(TextureCube* input)
  {
    auto computeShader = ShaderManager::LoadShader("SH_ComputeCube", RESOURCE_DIR "/shaders/ComputeMipCube.wgsl");

    auto device = RenderContext::GetDevice();

    auto bindGroupLayout = computeShader->GetReflectionInfo().LayoutDescriptors.begin()->second;
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &bindGroupLayout;
    const auto pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

    WGPUComputePipelineDescriptor pipelineDesc = {};
    pipelineDesc.layout = pipelineLayout;
    pipelineDesc.compute.entryPoint = RenderUtils::MakeLabel("computeMipMap");
    pipelineDesc.compute.module = computeShader->GetNativeShaderModule();
    const auto pipeline = wgpuDeviceCreateComputePipeline(device, &pipelineDesc);

    const uint32_t mipCount = RenderUtils::CalculateMipCount(
        input->GetSpec().Width,
        input->GetSpec().Height);

    for (uint32_t mipLevel = 1; mipLevel < mipCount; ++mipLevel)
    {
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

  struct PrefilterUniform
  {
    uint32_t currentMipLevel;
    uint32_t mipLevelCount;
    uint32_t _pad[2];
  };

  std::pair<Ref<TextureCube>, Ref<TextureCube>> RenderWGPU::CreateEnvironmentMap(const std::string& filepath)
  {
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

    RenderWGPU::ComputeEquirectToCubemap(envEquirect.get(), envUnfiltered.get());
    RenderWGPU::ComputeMipCube(envUnfiltered.get());

    RenderWGPU::ComputeEquirectToCubemap(envEquirect.get(), envFiltered.get());

    RenderWGPU::ComputePreFilter(envUnfiltered.get(), envFiltered.get());
    RenderWGPU::ComputeEnvironmentIrradiance(envFiltered.get(), irradianceMap.get());

    return std::make_pair(envFiltered, irradianceMap);
  }

  void RenderWGPU::ComputePreFilter(TextureCube* input, TextureCube* output)
  {
    auto computeShader = ShaderManager::LoadShader("SH_ComputeCube2", RESOURCE_DIR "/shaders/environment_prefilter.wgsl");

    const uint32_t mipCount = RenderUtils::CalculateMipCount(input->GetSpec().Width, input->GetSpec().Height);
    const uint32_t uniformStride = std::max((uint32_t)sizeof(PrefilterUniform), 256u);
    auto uniformBuffer = GPUAllocator::GAlloc(WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, uniformStride * mipCount);

    for (uint32_t mipLevel = 0; mipLevel < mipCount; ++mipLevel)
    {
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
    pipelineDesc.compute.entryPoint = RenderUtils::MakeLabel("prefilterCubeMap");
    pipelineDesc.compute.module = computeShader->GetNativeShaderModule();
    const auto pipeline = wgpuDeviceCreateComputePipeline(device, &pipelineDesc);

    for (uint32_t mipLevel = 0; mipLevel < mipCount; ++mipLevel)
    {
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

  void RenderWGPU::ComputeEnvironmentIrradiance(TextureCube* input, TextureCube* output)
  {
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
    pipelineDesc.compute.entryPoint = RenderUtils::MakeLabel("prefilterCubeMap");
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

  Ref<RenderPassEncoder> RenderWGPU::BeginRenderPass(Ref<RenderPass> pass, Ref<CommandEncoder> encoder)
  {
    RN_PROFILE_FUNC;
    pass->Prepare();

    const Ref<Framebuffer> renderFrameBuffer = pass->GetTargetFrameBuffer();

    WGPURenderPassDescriptor passDesc = {};
    ZERO_INIT(passDesc);
    passDesc.label = RenderUtils::MakeLabel(pass->GetProps().DebugName);
    passDesc.depthStencilAttachment = nullptr;
    passDesc.nextInChain = nullptr;

    // Declare attachments outside if-blocks so they remain valid until wgpuCommandEncoderBeginRenderPass
    WGPURenderPassColorAttachment colorAttachment{};
    WGPURenderPassDepthStencilAttachment depthAttachment{};

    if (renderFrameBuffer->HasColorAttachment())
    {
      ZERO_INIT(colorAttachment);
      colorAttachment.nextInChain = nullptr;
      colorAttachment.loadOp = WGPULoadOp_Load;
      colorAttachment.storeOp = WGPUStoreOp_Store;
      colorAttachment.clearValue = RenderWGPU::Instance->m_ClearColor;
      colorAttachment.resolveTarget = nullptr;
      colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

      if (renderFrameBuffer->m_FrameBufferSpec.SwapChainTarget)
      {
        Instance->m_SwapTexture = GetCurrentTextureView();
        colorAttachment.resolveTarget = Instance->m_SwapTexture;
      }

      colorAttachment.view = renderFrameBuffer->GetAttachment(0)->GetReadableView();
      passDesc.colorAttachmentCount = 1;
      passDesc.colorAttachments = &colorAttachment;
    }

    if (renderFrameBuffer->HasDepthAttachment())
    {
      ZERO_INIT(depthAttachment);

      auto depth = renderFrameBuffer->GetDepthAttachment();
      depthAttachment.depthClearValue = 1.0f;
      depthAttachment.depthLoadOp = WGPULoadOp_Clear;
      depthAttachment.depthStoreOp = WGPUStoreOp_Store;
      depthAttachment.depthReadOnly = false;

      // Browser WebGPU requires stencil ops undefined for depth-only formats
      // Depth24Plus has no stencil component
      depthAttachment.stencilClearValue = 0;
      depthAttachment.stencilLoadOp = WGPULoadOp_Undefined;
      depthAttachment.stencilStoreOp = WGPUStoreOp_Undefined;
      depthAttachment.stencilReadOnly = true;

      int layerNum = renderFrameBuffer->m_FrameBufferSpec.ExistingImageLayers.empty() ? 0 : renderFrameBuffer->m_FrameBufferSpec.ExistingImageLayers[0] + 1;
      depthAttachment.view = renderFrameBuffer->GetDepthAttachment()->GetReadableView(layerNum);

      passDesc.depthStencilAttachment = &depthAttachment;
    }

    const auto ptr = std::static_pointer_cast<CommandEncoderWGPU>(encoder);
    RN_ASSERT(ptr != nullptr);

    const WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(ptr->GetNativeEncoder(), &passDesc);
    const Ref<BindingManager> bindManager = pass->GetBindManager();

    for (const auto& [index, bindGroup] : bindManager->GetBindGroups())
    {
      wgpuRenderPassEncoderSetBindGroup(renderPass, index, bindGroup, 0, 0);
    }

    return CreateRef<RenderPassEncoderWGPU>(renderPass);
  }

  void RenderWGPU::EndRenderPass(Ref<RenderPass> pass, Ref<RenderPassEncoder> encoder)
  {
    RN_PROFILE_FUNC;

    const auto ptr = std::static_pointer_cast<RenderPassEncoderWGPU>(encoder);
    RN_ASSERT(ptr != nullptr);

    auto nativeEncoder = ptr->GetNativeEncoder();

    wgpuRenderPassEncoderEnd(nativeEncoder);
    wgpuRenderPassEncoderRelease(nativeEncoder);

    const auto renderFrameBuffer = pass->GetTargetFrameBuffer();

    if (renderFrameBuffer->m_FrameBufferSpec.SwapChainTarget)
    {
      wgpuTextureViewRelease(Instance->m_SwapTexture);
    }
  }

  void RenderWGPU::RenderMesh(Ref<RenderPassEncoder> renderCommandBuffer,
                              WGPURenderPipeline pipeline,
                              Ref<MeshSource> mesh,
                              uint32_t submeshIndex,
                              Ref<MaterialTable> materialTable,
                              Ref<GPUBuffer> transformBuffer,
                              uint32_t transformOffset,
                              uint32_t instanceCount)
  {
    const auto ptr = std::static_pointer_cast<RenderPassEncoderWGPU>(renderCommandBuffer);
    RN_ASSERT(ptr != nullptr);

    const auto nativeRenderPassEncoder = ptr->GetNativeEncoder();

    wgpuRenderPassEncoderSetPipeline(nativeRenderPassEncoder, pipeline);

    wgpuRenderPassEncoderSetVertexBuffer(nativeRenderPassEncoder,
                                         0,
                                         mesh->GetVertexBuffer()->Buffer,
                                         0,
                                         mesh->GetVertexBuffer()->Size);

    wgpuRenderPassEncoderSetIndexBuffer(nativeRenderPassEncoder,
                                        mesh->GetIndexBuffer()->Buffer,
                                        WGPUIndexFormat_Uint32,
                                        0,
                                        mesh->GetIndexBuffer()->Size);

    wgpuRenderPassEncoderSetVertexBuffer(nativeRenderPassEncoder,
                                         1,
                                         transformBuffer->Buffer,
                                         transformOffset,
                                         transformBuffer->Size - transformOffset);

    const auto& subMesh = mesh->m_SubMeshes[submeshIndex];
    auto material = materialTable->HasMaterial(subMesh.MaterialIndex) ? materialTable->GetMaterial(subMesh.MaterialIndex) : mesh->Materials->GetMaterial(subMesh.MaterialIndex);

    wgpuRenderPassEncoderSetBindGroup(nativeRenderPassEncoder, 1, material->GetBinding(1), 0, 0);
    wgpuRenderPassEncoderDrawIndexed(nativeRenderPassEncoder, subMesh.IndexCount, instanceCount, subMesh.BaseIndex, subMesh.BaseVertex, 0);
  }

  void RenderWGPU::SubmitFullscreenQuad(Ref<RenderPassEncoder> renderCommandBuffer, WGPURenderPipeline pipeline)
  {
    const auto ptr = std::static_pointer_cast<RenderPassEncoderWGPU>(renderCommandBuffer);
    RN_ASSERT(ptr != nullptr);

    const auto nativeRenderPassEncoder = ptr->GetNativeEncoder();

    wgpuRenderPassEncoderSetPipeline(nativeRenderPassEncoder, pipeline);
    wgpuRenderPassEncoderSetVertexBuffer(nativeRenderPassEncoder,
                                         0,
                                         s_Data->QuadVertexBuffer->Buffer,
                                         0,
                                         s_Data->QuadVertexBuffer->Size);

    wgpuRenderPassEncoderSetIndexBuffer(nativeRenderPassEncoder,
                                        s_Data->QuadIndexBuffer->Buffer,
                                        WGPUIndexFormat_Uint32,
                                        0,
                                        s_Data->QuadIndexBuffer->Size);
    wgpuRenderPassEncoderDrawIndexed(nativeRenderPassEncoder, 6, 1, 0, 0, 0);
  }

  Ref<CommandEncoder> RenderWGPU::CreateCommandEncoder()
  {
    WGPUCommandEncoderDescriptor commandEncoderDesc = {};
    ZERO_INIT(commandEncoderDesc);

    commandEncoderDesc.label = RenderUtils::MakeLabel("Default");
    commandEncoderDesc.nextInChain = nullptr;

    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(m_Device, &commandEncoderDesc);
    return CreateRef<CommandEncoderWGPU>(commandEncoder);
  }

  void RenderWGPU::SubmitAndFinishEncoder(Ref<CommandEncoder> commandEncoder)
  {
    const auto ptr = std::static_pointer_cast<CommandEncoderWGPU>(commandEncoder);
    RN_ASSERT(ptr != nullptr);

    const WGPUCommandEncoder nativeCommandEncoder = ptr->GetNativeEncoder();

    WGPUCommandBufferDescriptor cmdBufferDescriptor = {.nextInChain = nullptr, .label = "Command Buffer"};
    WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(nativeCommandEncoder, &cmdBufferDescriptor);

    wgpuQueueSubmit(m_Queue, 1, &commandBuffer);
    wgpuCommandBufferRelease(commandBuffer);
    wgpuCommandEncoderRelease(nativeCommandEncoder);
  }

  void RenderWGPU::Present()
  {
#ifndef __EMSCRIPTEN__
    wgpuSurfacePresent(m_Surface);
    wgpuDeviceTick(m_Device);
#endif
  }

  void RenderWGPU::ComputeEquirectToCubemap(Texture2D* equirectTexture, TextureCube* outputCubemap)
  {
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
    pipelineDesc.compute.entryPoint = RenderUtils::MakeLabel("main");
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

    wgpuBindGroupRelease(bindGroup);
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuComputePipelineRelease(pipeline);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(commandBuffer);
  }

  void RenderWGPU::Tick()
  {
#ifndef __EMSCRIPTEN__
    wgpuInstanceProcessEvents(Instance->m_DawnInstance);
#endif
  }

  void RenderWGPU::StartRequestInstance(void* nativeWindowPtr)
  {
#ifndef __EMSCRIPTEN__
    static const char* enabledTogglesArray[] = {
        "chromium_disable_uniformity_analysis",
        "allow_unsafe_apis"};

    WGPUDawnTogglesDescriptor dawnInstanceToggleDescriptor;
    ZERO_INIT(dawnInstanceToggleDescriptor);
    dawnInstanceToggleDescriptor.chain.sType = WGPUSType_DawnTogglesDescriptor;
    dawnInstanceToggleDescriptor.enabledToggles = enabledTogglesArray;
    dawnInstanceToggleDescriptor.enabledToggleCount = 2;
    dawnInstanceToggleDescriptor.disabledToggleCount = 0;

    WGPUInstanceDescriptor* gpuInstanceDescriptor = ZERO_ALLOC(WGPUInstanceDescriptor);
    gpuInstanceDescriptor->nextInChain = &dawnInstanceToggleDescriptor.chain;
    gpuInstanceDescriptor->requiredFeatureCount = 0;
    gpuInstanceDescriptor->requiredLimits = nullptr;
    gpuInstanceDescriptor->requiredFeatures = nullptr;

    DawnProcTable procs = dawn::native::GetProcs();
    dawnProcSetProcs(&procs);

    WGPUSurfaceDescriptor surfaceDescription{};
    const auto wnd = wgpu::glfw::SetupWindowAndGetSurfaceDescriptor(static_cast<GLFWwindow*>(nativeWindowPtr));
    surfaceDescription.nextInChain = reinterpret_cast<WGPUChainedStruct*>(wnd.get());

    m_DawnInstance = wgpuCreateInstance(gpuInstanceDescriptor);
    m_Window = static_cast<GLFWwindow*>(nativeWindowPtr);

    if (m_DawnInstance == nullptr)
    {
      RN_LOG_ERR("GPU Instance couldn't initialized");
    }

    m_Surface = wgpuInstanceCreateSurface(m_DawnInstance, &surfaceDescription);

    WGPURequestAdapterCallbackInfo adapterCallbackInfo;
    ZERO_INIT(adapterCallbackInfo);
    adapterCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents,
    adapterCallbackInfo.callback = &this->OnAdapterInstanceCallback;
    adapterCallbackInfo.userdata1 = this;

    WGPURequestAdapterOptions requestAdapterOpts;
    ZERO_INIT(requestAdapterOpts);
    requestAdapterOpts.compatibleSurface = m_Surface;
    requestAdapterOpts.powerPreference = WGPUPowerPreference_HighPerformance;

    wgpuInstanceRequestAdapter(m_DawnInstance, &requestAdapterOpts, adapterCallbackInfo);
#else
    // Emscripten WebGPU initialization
    WGPUInstanceDescriptor* instanceDescriptor = ZERO_ALLOC(WGPUInstanceDescriptor);
    if (instanceDescriptor == nullptr)
    {
      RN_LOG_ERR("WTF");
    }
    else
    {
      RN_LOG_ERR("WORKS!");
    }

    m_DawnInstance = wgpuCreateInstance(nullptr);

    if (m_DawnInstance == nullptr)
    {
      RN_LOG_ERR("GPU Instance couldn't initialized");
      return;
    }

    // Create surface from HTML canvas
    WGPUSurfaceDescriptorFromCanvasHTMLSelector* canvasDesc = ZERO_ALLOC(WGPUSurfaceDescriptorFromCanvasHTMLSelector);
    canvasDesc->chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
    canvasDesc->selector = "#canvas";

    WGPUSurfaceDescriptor* surfaceDesc = ZERO_ALLOC(WGPUSurfaceDescriptor);
    surfaceDesc->nextInChain = &canvasDesc->chain;

    m_Surface = wgpuInstanceCreateSurface(m_DawnInstance, surfaceDesc);
    //
    m_Window = (GLFWwindow*)m_Surface;

    WGPURequestAdapterOptions* requestAdapterOpts = ZERO_ALLOC(WGPURequestAdapterOptions);
    requestAdapterOpts->compatibleSurface = m_Surface;
    requestAdapterOpts->powerPreference = WGPUPowerPreference_HighPerformance;

    wgpuInstanceRequestAdapter(
        m_DawnInstance,
        requestAdapterOpts,
        [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata)
        {
          if (status != WGPURequestAdapterStatus_Success)
          {
            RN_LOG_ERR("Failed to request adapter: {}", message);
            return;
          }
          auto* self = static_cast<RenderWGPU*>(userdata);
          self->m_Adapter = adapter;
          self->StartRequestDevice();
        },
        this);
#endif
  }

  void RenderWGPU::StartRequestDevice()
  {
#ifndef __EMSCRIPTEN__
    static auto uncapturedErrorCallback = [](WGPUDevice const* device,
                                             WGPUErrorType type,
                                             WGPUStringView message,
                                             void* userdata1,
                                             void* userdata2)
    {
      const char* errorTypeName = "";
      switch (type)
      {
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

    WGPUUncapturedErrorCallbackInfo uncapturedErrorInfo;
    ZERO_INIT(uncapturedErrorInfo);
    uncapturedErrorInfo.callback = uncapturedErrorCallback;
#endif

#ifndef __EMSCRIPTEN__
    static auto deviceLostCallback = [](WGPUDevice const* device,
                                        WGPUDeviceLostReason reason,
                                        WGPUStringView message,
                                        void* userdata1,
                                        void* userdata2)
    {
      RN_LOG_ERR("GPU device lost: len: {} data: {} \n", (int)message.length, message.data);
    };

    WGPUDeviceLostCallbackInfo deviceLostInfo;
    ZERO_INIT(deviceLostInfo);
    deviceLostInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    deviceLostInfo.callback = deviceLostCallback;

    WGPURequestDeviceCallbackInfo deviceCallbackInfo;
    ZERO_INIT(deviceCallbackInfo);
    deviceCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    deviceCallbackInfo.callback = &RenderWGPU::OnDeviceCallback;
    deviceCallbackInfo.userdata1 = this;
#else
    static auto deviceLostCallback = [](WGPUDeviceLostReason reason,
                                        WGPUStringView message,
                                        void* userdata1)
    {
      RN_LOG_ERR("GPU device lost: len: {} data: {} \n", sizeof(message), message);
    };
#endif

    std::vector<WGPUFeatureName> requiredFeatures = {
        WGPUFeatureName_TimestampQuery,
        WGPUFeatureName_TextureCompressionBC,
        WGPUFeatureName_Float32Filterable,
        WGPUFeatureName_DepthClipControl};

#ifndef __EMSCRIPTEN__
    WGPULimits* requiredLimits = ZERO_ALLOC(WGPULimits);
    requiredLimits->minUniformBufferOffsetAlignment = 256;
    requiredLimits->minStorageBufferOffsetAlignment = 256;
    requiredLimits->maxComputeInvocationsPerWorkgroup = 1024;
#else
    WGPUSupportedLimits* supportedLimits = ZERO_ALLOC(WGPUSupportedLimits);
    wgpuAdapterGetLimits(m_Adapter, supportedLimits);

    WGPURequiredLimits* requiredLimits = ZERO_ALLOC(WGPURequiredLimits);
    requiredLimits->limits = supportedLimits->limits;
    requiredLimits->limits.minUniformBufferOffsetAlignment = 256;
    requiredLimits->limits.minStorageBufferOffsetAlignment = 256;
    requiredLimits->limits.maxComputeInvocationsPerWorkgroup = 1024;
    requiredLimits->limits.maxInterStageShaderComponents = WGPU_LIMIT_U32_UNDEFINED;
#endif

    WGPUDeviceDescriptor* gpuDeviceDescriptor = ZERO_ALLOC(WGPUDeviceDescriptor);
    gpuDeviceDescriptor->label = RenderUtils::MakeLabel("MyDevice");
    gpuDeviceDescriptor->requiredFeatures = requiredFeatures.data();
    gpuDeviceDescriptor->requiredFeatureCount = requiredFeatures.size();
    gpuDeviceDescriptor->requiredLimits = requiredLimits;
    gpuDeviceDescriptor->defaultQueue.label = RenderUtils::MakeLabel("defq");

#ifndef __EMSCRIPTEN__
    gpuDeviceDescriptor->uncapturedErrorCallbackInfo = uncapturedErrorInfo;
    gpuDeviceDescriptor->deviceLostCallbackInfo = deviceLostInfo;
#else
    gpuDeviceDescriptor->deviceLostCallback = deviceLostCallback;
#endif

#ifndef __EMSCRIPTEN__
    wgpuAdapterRequestDevice(m_Adapter, gpuDeviceDescriptor, deviceCallbackInfo);
#else
    wgpuAdapterRequestDevice(m_Adapter, gpuDeviceDescriptor, &RenderWGPU::OnDeviceCallback, this);
#endif
  }

  void RenderWGPU::Initialize()
  {
    m_Queue = wgpuDeviceGetQueue(m_Device);

    RN_CORE_ASSERT(m_Queue, "An error occurred while acquiring the queue. This might indicate unsupported browser/device.");
    RN_CORE_ASSERT(m_Device, "An error occurred while acquiring the Device.");

    m_RenderContext = CreateRef<RenderContext>(m_Adapter, m_Surface, m_Device, m_Queue);

    WGPUSurfaceCapabilities* capabilities = new WGPUSurfaceCapabilities();
    wgpuSurfaceGetCapabilities(m_Surface, m_Adapter, capabilities);

    m_swapChainFormat = WGPUTextureFormat_BGRA8Unorm;

    int width, height;
    glfwGetFramebufferSize(m_Window, &width, &height);

    WGPUSurfaceConfiguration config = {};
    config.device = m_Device;
    config.format = m_swapChainFormat;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;
    config.width = width;
    config.height = height;
    config.presentMode = WGPUPresentMode_Fifo;
    config.viewFormatCount = 1;
    config.viewFormats = &m_swapChainFormat;
    wgpuSurfaceConfigure(m_Surface, &config);

    s_Data = new WGPURendererData();

    float x = -1;
    float y = -1;
    float qwidth = 2, qheight = 2;
    struct QuadVertex
    {
      glm::vec3 Position;
      float _pad0 = 0;
      glm::vec2 TexCoord;
      float _pad1[2] = {0, 0};
    };

    QuadVertex* data = new QuadVertex[4];

    data[0].Position = glm::vec3(x, y, 0.0f);  // Bottom-left
    data[0].TexCoord = glm::vec2(0, 1);

    data[1].Position = glm::vec3(x + qwidth, y, 0.0f);  // Bottom-right
    data[1].TexCoord = glm::vec2(1, 1);

    data[2].Position = glm::vec3(x + qwidth, y + qheight, 0.0f);  // Top-right
    data[2].TexCoord = glm::vec2(1, 0);

    data[3].Position = glm::vec3(x, y + qheight, 0.0f);  // Top-left
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

    if (OnReady != nullptr)
    {
      OnReady();
    }
  }

#ifndef __EMSCRIPTEN__
  void RenderWGPU::OnDeviceCallback(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* userdata2)
#else
  void RenderWGPU::OnDeviceCallback(WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* userdata1)
#endif
  {
    auto* self = static_cast<RenderWGPU*>(userdata1);
    if (!self)
    {
      return;
    }

    if (status == WGPURequestDeviceStatus_Success)
    {
      RN_LOG("Device request successful");
      self->m_Device = device;
      self->Initialize();
    }
    else
    {
#ifndef __EMSCRIPTEN__
      std::string errorMessage(message.data, message.length);
      RN_LOG_ERR("Failed to request adapter: {}", errorMessage.c_str());
#else
      RN_LOG_ERR("Failed to request device: {}", message);
#endif
    }
  }

#ifndef __EMSCRIPTEN__
  void RenderWGPU::OnAdapterInstanceCallback(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2)
  {
    auto* self = static_cast<RenderWGPU*>(userdata1);
    if (!self)
    {
      return;
    }

    auto* thisClass = static_cast<Render*>(userdata1);

    if (status == WGPURequestAdapterStatus_Success)
    {
      self->m_Adapter = adapter;
      RN_LOG("Adapter request successful");
      self->StartRequestDevice();
    }
    else
    {
      std::string errorMessage(message.data, message.length);
      RN_LOG_ERR("Failed to request adapter: {}", errorMessage.c_str());
    }
  }
#endif
}  // namespace Rain
