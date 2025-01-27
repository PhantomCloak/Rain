#include "Pipeline.h"
#include "render/Render.h"
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

RenderPipeline::RenderPipeline(const RenderPipelineSpec& props)
    : m_PipelineSpec(props) {
  Invalidate();
	Render::RegisterShaderDependency(props.Shader, this);
}

void RenderPipeline::Invalidate() {
  //if (m_Pipeline != nullptr) {
  //  wgpuRenderPipelineRelease(m_Pipeline);
  //}

  WGPURenderPipelineDescriptor* pipelineDesc = (WGPURenderPipelineDescriptor*)malloc(sizeof(WGPURenderPipelineDescriptor));
  pipelineDesc->label = m_PipelineSpec.DebugName.c_str();

  std::vector<WGPUVertexAttribute> vertexAttributes;
  std::vector<WGPUVertexAttribute> instanceAttributes;

  std::vector<WGPUVertexBufferLayout> vertexLayouts;

  const auto& vertexLayout = m_PipelineSpec.VertexLayout;
  const auto& instanceLayout = m_PipelineSpec.InstanceLayout;

  for (const auto& element : vertexLayout) {
    vertexAttributes.push_back({.format = ConvertWGPUVertexFormat(element.Type),
                                .offset = element.Offset,
                                .shaderLocation = element.Location});
  }

  WGPUVertexBufferLayout vblVertex = {};
  vblVertex.attributeCount = vertexAttributes.size();
  vblVertex.attributes = vertexAttributes.data();
  vblVertex.arrayStride = vertexLayout.GetStride();
  vblVertex.stepMode = WGPUVertexStepMode_Vertex;

  vertexLayouts.push_back(vblVertex);
  if (instanceLayout.GetElementCount()) {
    for (const auto& element : instanceLayout) {
      instanceAttributes.push_back({.format = ConvertWGPUVertexFormat(element.Type),
                                    .offset = element.Offset,
                                    .shaderLocation = element.Location});
    }

    WGPUVertexBufferLayout vblInstance = {};
    vblInstance.attributeCount = instanceAttributes.size();
    vblInstance.attributes = instanceAttributes.data();
    vblInstance.arrayStride = instanceLayout.GetStride();
    vblInstance.stepMode = WGPUVertexStepMode_Instance;

    vertexLayouts.push_back(vblInstance);
  }

  //// TODO: Inspect memory for emscriptten
  pipelineDesc->vertex.bufferCount = vertexLayouts.size();
  pipelineDesc->vertex.buffers = vertexLayouts.data();
  pipelineDesc->vertex.module = m_PipelineSpec.Shader->GetNativeShaderModule();
  pipelineDesc->vertex.entryPoint = "vs_main";  // let be constant for now
  pipelineDesc->vertex.nextInChain = nullptr;
  pipelineDesc->primitive.topology = WGPUPrimitiveTopology_TriangleList;
  pipelineDesc->primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
  pipelineDesc->primitive.frontFace = WGPUFrontFace_CCW;
  pipelineDesc->primitive.nextInChain = nullptr;

  std::vector<WGPUConstantEntry>* constants = new std::vector<WGPUConstantEntry>();

  for (const auto& [key, value] : m_PipelineSpec.Overrides) {
    WGPUConstantEntry constant;
    constant.key = key.c_str();
    constant.value = static_cast<double>(value);
    constant.nextInChain = nullptr;
    constants->push_back(constant);
  }
  pipelineDesc->vertex.constantCount = constants->size();
  pipelineDesc->vertex.constants = constants->size() > 0 ? constants->data() : NULL;

  WGPUCullMode mode;
  if (m_PipelineSpec.CullingMode == PipelineCullingMode::BACK) {
    mode = WGPUCullMode_Back;
  } else if (m_PipelineSpec.CullingMode == PipelineCullingMode::FRONT) {
    mode = WGPUCullMode_Front;
  } else {
    mode = WGPUCullMode_None;
  }

  pipelineDesc->primitive.cullMode = mode;

  WGPUFragmentState* fragmentState = (WGPUFragmentState*)malloc(sizeof(WGPUFragmentState));
  fragmentState->module = m_PipelineSpec.Shader->GetNativeShaderModule();
  fragmentState->entryPoint = "fs_main";
  fragmentState->constantCount = 0;
  fragmentState->constants = NULL;
  fragmentState->nextInChain = nullptr;

  if (m_PipelineSpec.TargetFramebuffer->HasColorAttachment()) {
    WGPUBlendState* blendState = (WGPUBlendState*)malloc(sizeof(WGPUBlendState));
    blendState->color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState->color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState->color.operation = WGPUBlendOperation_Add;
    blendState->alpha.srcFactor = WGPUBlendFactor_Zero;
    blendState->alpha.dstFactor = WGPUBlendFactor_One;
    blendState->alpha.operation = WGPUBlendOperation_Add;

    WGPUColorTargetState* colorTarget = (WGPUColorTargetState*)malloc(sizeof(WGPUColorTargetState));
    colorTarget->format = RenderTypeUtils::ToRenderType(m_PipelineSpec.TargetFramebuffer->m_FrameBufferSpec.ColorFormat);
    colorTarget->blend = blendState;
    colorTarget->writeMask = WGPUColorWriteMask_All;
    colorTarget->nextInChain = nullptr;

    fragmentState->targets = colorTarget;
  };

  fragmentState->targetCount = m_PipelineSpec.TargetFramebuffer->HasColorAttachment() ? 1 : 0;

  if (m_PipelineSpec.TargetFramebuffer->HasDepthAttachment()) {
    WGPUDepthStencilState* depthStencilState = (WGPUDepthStencilState*)malloc(sizeof(WGPUDepthStencilState));
    depthStencilState->format = WGPUTextureFormat_Depth24Plus;
    depthStencilState->stencilReadMask = 0xFFFFFFFF;
    depthStencilState->stencilWriteMask = 0xFFFFFFFF;

    depthStencilState->depthBias = 0;
    depthStencilState->depthBiasSlopeScale = 0;
    depthStencilState->depthBiasClamp = 0;

    depthStencilState->stencilFront.compare = WGPUCompareFunction_Always;
    depthStencilState->stencilFront.failOp = WGPUStencilOperation_Keep;
    depthStencilState->stencilFront.depthFailOp = WGPUStencilOperation_Keep;
    depthStencilState->stencilFront.passOp = WGPUStencilOperation_Keep;

    depthStencilState->stencilBack.compare = WGPUCompareFunction_Always;
    depthStencilState->stencilBack.failOp = WGPUStencilOperation_Keep;
    depthStencilState->stencilBack.depthFailOp = WGPUStencilOperation_Keep;
    depthStencilState->stencilBack.passOp = WGPUStencilOperation_Keep;

    depthStencilState->depthCompare = WGPUCompareFunction_Less;
    depthStencilState->depthWriteEnabled = true;

    depthStencilState->stencilReadMask = 0xFFFFFFFF;
    depthStencilState->stencilWriteMask = 0xFFFFFFFF;
    depthStencilState->nextInChain = nullptr;

    pipelineDesc->depthStencil = depthStencilState;
  } else {
    pipelineDesc->depthStencil = nullptr;
  }

  pipelineDesc->fragment = fragmentState;
  pipelineDesc->multisample.count = 1;
  pipelineDesc->multisample.mask = ~0u;
  pipelineDesc->multisample.alphaToCoverageEnabled = false;
  pipelineDesc->multisample.nextInChain = nullptr;

  if (m_PipelineSpec.DebugName == "RP_Composite" || m_PipelineSpec.DebugName == "RP_Skybox") {
    pipelineDesc->multisample.count = 4;
  }

  auto device = RenderContext::GetDevice();

  std::vector<WGPUBindGroupLayout> bindGroupLayouts;
  for (const auto& [_, layout] : m_PipelineSpec.Shader->GetReflectionInfo().LayoutDescriptors) {
    bindGroupLayouts.push_back(layout);
  }
  WGPUPipelineLayoutDescriptor* layoutDesc = (WGPUPipelineLayoutDescriptor*)malloc(sizeof(WGPUPipelineLayoutDescriptor));
  layoutDesc->bindGroupLayoutCount = bindGroupLayouts.size();
  layoutDesc->bindGroupLayouts = bindGroupLayouts.data();
  layoutDesc->nextInChain = nullptr;

  WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, layoutDesc);
  pipelineDesc->layout = pipelineLayout;
  pipelineDesc->nextInChain = nullptr;

  RN_LOG("CREATE: {}", m_PipelineSpec.DebugName);
  m_Pipeline = wgpuDeviceCreateRenderPipeline(device, pipelineDesc);
}
