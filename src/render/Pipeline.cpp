#include "Pipeline.h"
#include "render/RenderContext.h"

// In some environments (such as Emscriptten) underlying implementation details of the WebGPU API might require HEAP pointers
struct PipelineBundle {
  WGPURenderPipelineDescriptor pipelineDescriptor;
  WGPUColorTargetState colorTargetState;
  WGPUFragmentState fragmentState;
  WGPUBlendState blendState;
  WGPUDepthStencilState depthStencilState;
  std::vector<WGPUBindGroupLayout> bindGroupLayouts;
  std::vector<WGPUBindGroupLayoutDescriptor> bindGroupLayoutDescriptors;
  std::map<int, std::vector<WGPUBindGroupLayoutEntry>> groupLayoutEntries;
  WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor;
  WGPUPipelineLayout pipelineLayout;
  WGPURenderPipeline pipeline;
};

RenderPipeline::RenderPipeline(std::string name, RenderPipelineProps props, const std::vector<WGPUVertexBufferLayout>& vertexLayout, std::map<int, GroupLayout>& groupLayout) {
  WGPUTextureFormat swapChainFormat = WGPUTextureFormat_Undefined;

#if __EMSCRIPTEN__
  swapChainFormat = wgpuSurfaceGetPreferredFormat(surface, adapter);
#else
  swapChainFormat = WGPUTextureFormat_BGRA8Unorm;
#endif

  static std::map<std::string, PipelineBundle> pipelineBundle = {};

  PipelineBundle& bundle = pipelineBundle[name];
	m_PipelineProps = props;

  WGPURenderPipelineDescriptor& pipelineDesc = bundle.pipelineDescriptor;

  pipelineDesc.label = name.c_str();

  // TODO: Inspect memory for emscriptten
  pipelineDesc.vertex.bufferCount = vertexLayout.size();
  pipelineDesc.vertex.buffers = vertexLayout.data();

  pipelineDesc.vertex.module = props.VertexShader;
  pipelineDesc.vertex.entryPoint = "vs_main";  // let be constant for now

  pipelineDesc.vertex.constantCount = 0;
  pipelineDesc.vertex.constants = nullptr;

  pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
  pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;

  WGPUCullMode mode;
  if (props.CullingMode == PipelineCullingMode::BACK) {
    mode = WGPUCullMode_Back;
  } else if (props.CullingMode == PipelineCullingMode::FRONT) {
    mode = WGPUCullMode_Front;
  } else {
    mode = WGPUCullMode_None;
  }

  pipelineDesc.primitive.cullMode = mode;

  WGPUFragmentState& fragmentState = bundle.fragmentState;
  fragmentState.module = props.FragmentShader;
  fragmentState.entryPoint = "fs_main";
  fragmentState.constantCount = 0;
  fragmentState.constants = NULL;

  if (props.ColorFormat != TextureFormat::Undefined) {
    WGPUBlendState& blendState = bundle.blendState;
    blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.color.operation = WGPUBlendOperation_Add;
    blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
    blendState.alpha.dstFactor = WGPUBlendFactor_One;
    blendState.alpha.operation = WGPUBlendOperation_Add;

    WGPUColorTargetState& colorTarget = bundle.colorTargetState;
    colorTarget.format = swapChainFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
  } else {
    fragmentState.targetCount = 0;
  }

  pipelineDesc.fragment = &fragmentState;

  // The Depth Stencil State defines how the pipeline will handle depth testing
  // and stencil testing. Use cases:
  // - Depth Testing for 3D Scenes
  // - Stencil Operations such as Shadow-Mapping

  if (props.DepthFormat == TextureFormat::Undefined) {
    pipelineDesc.depthStencil = nullptr;
  } else {
    WGPUDepthStencilState& depthStencilState = bundle.depthStencilState;
    depthStencilState.format = WGPUTextureFormat_Depth24Plus;
    depthStencilState.stencilReadMask = 0xFFFFFFFF;
    depthStencilState.stencilWriteMask = 0xFFFFFFFF;

    depthStencilState.depthBias = 0;
    depthStencilState.depthBiasSlopeScale = 0;
    depthStencilState.depthBiasClamp = 0;

    depthStencilState.stencilFront.compare = WGPUCompareFunction_Always;
    depthStencilState.stencilFront.failOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilFront.depthFailOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilFront.passOp = WGPUStencilOperation_Keep;

    depthStencilState.stencilBack.compare = WGPUCompareFunction_Always;
    depthStencilState.stencilBack.failOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilBack.depthFailOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilBack.passOp = WGPUStencilOperation_Keep;

    depthStencilState.depthCompare = WGPUCompareFunction_Less;
    depthStencilState.depthWriteEnabled = true;

    // Resetting stencil masks to 0, which might be a mistake. Usually, you would
    // leave these as 0xFFFFFFFF unless you have a specific reason to change them.
    depthStencilState.stencilReadMask = 0xFFFFFFFF;
    depthStencilState.stencilWriteMask = 0xFFFFFFFF;

    pipelineDesc.depthStencil = &depthStencilState;
  }

  pipelineDesc.multisample.count = 1;
  pipelineDesc.multisample.mask = ~0u;
  pipelineDesc.multisample.alphaToCoverageEnabled = false;

  std::vector<WGPUBindGroupLayout>& bindGroupLayouts = bundle.bindGroupLayouts;

	auto device = RenderContext::GetDevice();
  for (int i = 0; i < groupLayout.size(); i++) {
    WGPUBindGroupLayout layout = LayoutUtils::CreateBindGroup("bgl_" + name + std::to_string(i), device, groupLayout[i]);
    bindGroupLayouts.push_back(layout);
  }

  WGPUPipelineLayoutDescriptor& layoutDesc = bundle.pipelineLayoutDescriptor;

  layoutDesc.bindGroupLayoutCount = bindGroupLayouts.size();
  layoutDesc.bindGroupLayouts = bindGroupLayouts.data();

  WGPUPipelineLayout& pipelineLayout = bundle.pipelineLayout;
  pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &layoutDesc);
  pipelineDesc.layout = pipelineLayout;

	m_Pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
}
