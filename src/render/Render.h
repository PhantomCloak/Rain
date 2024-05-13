#pragma once
#include <webgpu/webgpu.h>
#include <memory>
#include "Example.h"

class Render {
 public:
	static Render* Instance;
  WGPUInstance CreateInstance();
  WGPUDevice RequestDevice();
  bool Init(void* window, WGPUInstance instance, WGPUSurface surface);
  void SetClearColor(float r, float g, float b, float a);
  void SetPipeline(WGPURenderPipeline pipeline);
  void OnFrameStart();
  void OnFrameEnd();

  WGPUAdapter requestAdapter(WGPUInstance instance, WGPURequestAdapterOptions const* options);
  WGPUDevice requestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor);
  WGPUSwapChainDescriptor GetSwapchainDescriptor(int width, int height, WGPUTextureFormat swapChainFormat);
  WGPURequiredLimits GetRequiredLimits(WGPUAdapter adapter);
  WGPUSwapChain buildSwapChain(WGPUSwapChainDescriptor descriptor, WGPUDevice device, WGPUSurface surface);
  WGPUTexture GetDepthBufferTexture(WGPUDevice device, WGPUSwapChainDescriptor descriptor);
  WGPUTextureView GetDepthBufferTextureView(WGPUTexture& depthTexture, WGPUTextureFormat depthTextureFormat);
  WGPUVertexBufferLayout GetVertexBufferLayout();
  void AttachFragmentStateToPipeline(WGPURenderPipelineDescriptor* pipe, WGPUShaderModule& shaderModule, WGPUTextureFormat swapChainFormat);
  void AttachDepthStencilStateToPipeline(WGPURenderPipelineDescriptor& pipe, WGPUTextureFormat depthTextureFormat);
  WGPUBindGroupLayout* SetupBindingLayouts(WGPURenderPipelineDescriptor* pipe, WGPUDevice device);
  WGPUSampler AttachSampler(WGPUDevice device);

  WGPUAdapter m_adapter = nullptr;
  WGPUDevice m_device = nullptr;
  WGPUQueue m_queue = nullptr;
  WGPUTexture m_depthTexture = nullptr;
  WGPUTextureView m_depthTextureView = nullptr;
  WGPUTextureView m_depthTextureView2 = nullptr;
  WGPUSwapChain m_swapChain = nullptr;
  WGPUSampler m_sampler = nullptr;
  WGPUColor m_Color;
  WGPUTextureView nextTexture = nullptr;
  WGPURenderPassEncoder renderPass;
  WGPUCommandEncoder encoder;
  WGPUTextureFormat m_swapChainFormat = WGPUTextureFormat_Undefined;
  //WGPUTextureFormat m_depthTextureFormat = WGPUTextureFormat_Depth24Plus;
  WGPUTextureFormat m_depthTextureFormat = WGPUTextureFormat_Depth32Float;
  WGPUSwapChainDescriptor m_swapChainDesc;
  std::unique_ptr<PipelineManager> m_PipelineManager;
  std::shared_ptr<ShaderManager> m_ShaderManager;
};
