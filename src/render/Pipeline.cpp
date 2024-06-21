#include "Pipeline.h"
#include "render/RenderContext.h"

// In some environments (such as Emscriptten) underlying implementation details of the WebGPU API might require HEAP pointers

WGPUVertexFormat ConvertWGPUVertexFormat(ShaderDataType type) {
  switch (type) {
    case ShaderDataType::Float:
      return WGPUVertexFormat_Float32;
    case ShaderDataType::Float2:
      return WGPUVertexFormat_Float32x2;
    case ShaderDataType::Float3:
      return WGPUVertexFormat_Float32x3;
    case ShaderDataType::Float4:
      return WGPUVertexFormat_Float32x4;
    case ShaderDataType::Mat3:
      RN_CORE_ASSERT("Vertex format cannot be matrix.");
    case ShaderDataType::Mat4:
      RN_CORE_ASSERT("Vertex format cannot be matrix.");
    case ShaderDataType::Int:
      return WGPUVertexFormat_Uint32;
    case ShaderDataType::Int2:
      return WGPUVertexFormat_Uint32x2;
    case ShaderDataType::Int3:
      return WGPUVertexFormat_Uint32x3;
    case ShaderDataType::Int4:
      return WGPUVertexFormat_Uint32x4;
    case ShaderDataType::Bool:
      RN_CORE_ASSERT("Vertex format cannot be boolean");
    case ShaderDataType::None:
      RN_CORE_ASSERT("Invalid Vertex Format");
      break;
  }
  RN_CORE_ASSERT("Undefined Vertex Format");
  return WGPUVertexFormat_Undefined;
}

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

RenderPipeline::RenderPipeline(std::string name, const RenderPipelineProps& props, Ref<Texture> colorAttachment, Ref<Texture> depthAttachment)
    : m_PipelineProps(props), m_ColorAttachment(colorAttachment), m_DepthAttachment(depthAttachment) {
  WGPUTextureFormat swapChainFormat = WGPUTextureFormat_Undefined;

#if __EMSCRIPTEN__
  swapChainFormat = wgpuSurfaceGetPreferredFormat(RenderContext::GetSurface(), RenderContext::GetAdapter());
#else
  swapChainFormat = WGPUTextureFormat_BGRA8Unorm;
#endif

  static std::map<std::string, PipelineBundle> pipelineBundle = {};

  PipelineBundle& bundle = pipelineBundle[name];
  m_PipelineProps = props;

  WGPURenderPipelineDescriptor& pipelineDesc = bundle.pipelineDescriptor;

  pipelineDesc.label = name.c_str();

  std::vector<WGPUVertexAttribute> vertexAttributes;
  std::vector<WGPUVertexAttribute> instanceAttributes;

  const auto& vertexLayout = m_PipelineProps.VertexLayout;
  const auto& instanceLayout = m_PipelineProps.InstanceLayout;

  for (const auto& element : vertexLayout) {
    vertexAttributes.push_back({.format = ConvertWGPUVertexFormat(element.Type),
                                .offset = element.Offset,
                                .shaderLocation = element.Location});
  }

  for (const auto& element : instanceLayout) {
    instanceAttributes.push_back({.format = ConvertWGPUVertexFormat(element.Type),
                                  .offset = element.Offset,
                                  .shaderLocation = element.Location});
  }

  WGPUVertexBufferLayout vblVertex = {};
  vblVertex.attributeCount = vertexAttributes.size();
  vblVertex.attributes = vertexAttributes.data();
  vblVertex.arrayStride = vertexLayout.GetStride();
  vblVertex.stepMode = WGPUVertexStepMode_Vertex;

  WGPUVertexBufferLayout vblInstance = {};
  vblInstance.attributeCount = instanceAttributes.size();
  vblInstance.attributes = instanceAttributes.data();
  vblInstance.arrayStride = instanceLayout.GetStride();
  vblInstance.stepMode = WGPUVertexStepMode_Instance;

  std::vector<WGPUVertexBufferLayout> vertexLayouts;
  vertexLayouts.push_back(vblVertex);
  vertexLayouts.push_back(vblInstance);

  // TODO: Inspect memory for emscriptten
  pipelineDesc.vertex.bufferCount = vertexLayouts.size();
  pipelineDesc.vertex.buffers = vertexLayouts.data();

  pipelineDesc.vertex.module = props.VertexShader->GetNativeShaderModule();
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
  fragmentState.module = props.FragmentShader->GetNativeShaderModule();
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

    depthStencilState.stencilReadMask = 0xFFFFFFFF;
    depthStencilState.stencilWriteMask = 0xFFFFFFFF;

    pipelineDesc.depthStencil = &depthStencilState;
  }

  pipelineDesc.multisample.count = 1;
  pipelineDesc.multisample.mask = ~0u;
  pipelineDesc.multisample.alphaToCoverageEnabled = false;

  std::vector<WGPUBindGroupLayout>& bindGroupLayouts = bundle.bindGroupLayouts;

  auto device = RenderContext::GetDevice();
  auto& groupLayout = m_PipelineProps.groupLayout;

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
