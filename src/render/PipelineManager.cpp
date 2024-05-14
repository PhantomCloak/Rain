#include "PipelineManager.h"


WGPUVertexBufferLayout CreateVertexBufferLayout(BufferLayout layout) {
  // TODO: Introduce clear
  static std::vector<std::vector<WGPUVertexAttribute>> vertexAttribStore;
  vertexAttribStore.emplace_back(
      std::vector<WGPUVertexAttribute>(layout.GetElementCount()));
  std::vector<WGPUVertexAttribute>& vertexAttribs =
      vertexAttribStore[vertexAttribStore.size() - 1];

  int ctx = 0;
  for (const auto& attrib : layout) {
    WGPUVertexAttribute attribute = {};
    attribute.shaderLocation = ctx;

    if (attrib.Type == ShaderDataType::Float3) {
      attribute.format = WGPUVertexFormat_Float32x3;
    } else if (attrib.Type == ShaderDataType::Float2) {
      attribute.format = WGPUVertexFormat_Float32x2;
    }

    attribute.offset = attrib.Offset;

    vertexAttribs[ctx] = attribute;
    ctx++;
  }

  WGPUVertexBufferLayout vertexBufferLayout = {};
  vertexBufferLayout.attributeCount =
      static_cast<uint32_t>(vertexAttribs.size());
  vertexBufferLayout.attributes = vertexAttribs.data();
  vertexBufferLayout.arrayStride = layout.GetStride();
  vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

  return vertexBufferLayout;
}

void SetVisibility(WGPUBindGroupLayoutEntry& entry,
                   GroupLayoutVisibility visibility) {
  switch (visibility) {
    case GroupLayoutVisibility::Vertex:
      entry.visibility = WGPUShaderStage_Vertex;
      break;
    case GroupLayoutVisibility::Fragment:
      entry.visibility = WGPUShaderStage_Fragment;
      break;
    case GroupLayoutVisibility::Both:
      entry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
      break;
  }
}

void SetType(WGPUBindGroupLayoutEntry& entry, GroupLayoutType type) {
  switch (type) {
    case GroupLayoutType::Default:
      entry.buffer.type = WGPUBufferBindingType_Uniform;
      break;
    case GroupLayoutType::Texture:
      entry.texture.sampleType = WGPUTextureSampleType_Float;
      entry.texture.viewDimension = WGPUTextureViewDimension_2D;
      break;
    case GroupLayoutType::Sampler:
      entry.sampler.type = WGPUSamplerBindingType_Filtering;
      break;
  }
}

std::map<int, std::vector<WGPUBindGroupLayoutEntry>>
ParseGroupLayout(GroupLayout layout) {
  std::map<int, std::vector<WGPUBindGroupLayoutEntry>> groups;
  std::map<int, int> groupsMemberCtx;

  // We may need to account for buffer.minBindingSize
  for (auto layoutItem : layout) {
    WGPUBindGroupLayoutEntry entry = {};

    entry.binding = groupsMemberCtx[layoutItem.Binding]++;
    SetVisibility(entry, layoutItem.Visiblity);
    SetType(entry, layoutItem.Type);

    groups[layoutItem.Binding].push_back(entry);
  }

  return groups;
}

WGPURenderPipeline PipelineManager::CreatePipeline(
    const std::string& pipelineId,
    const std::string& shaderId,
    BufferLayout vertexLayout,
    GroupLayout groupLayout,
    WGPUTextureFormat depthFormat,
    WGPUSurface surface,
    WGPUAdapter adapter) {
  WGPUShaderModule shader = shaderManager_->GetShader(shaderId);
  WGPUVertexBufferLayout vertexBufferLayout =
      CreateVertexBufferLayout(vertexLayout);

  WGPUTextureFormat swapChainFormat = WGPUTextureFormat_Undefined;

#if __EMSCRIPTEN__
  swapChainFormat = wgpuSurfaceGetPreferredFormat(surface, adapter);
#else
  swapChainFormat = WGPUTextureFormat_BGRA8Unorm;
#endif

  static std::map<std::string, WGPURenderPipelineDescriptor> pipelineDescList = {};

  if (pipelineDescList.find(pipelineId) == pipelineDescList.end()) {
    pipelineDescList[pipelineId] = {};
  }

  WGPURenderPipelineDescriptor& pipelineDesc = pipelineDescList[pipelineId];

  pipelineDesc.label = pipelineId.c_str();

  pipelineDesc.vertex.bufferCount = 1;
  pipelineDesc.vertex.buffers = &vertexBufferLayout;

  pipelineDesc.vertex.module = shader;
  pipelineDesc.vertex.entryPoint = "vs_main";  // let be constant for now

  pipelineDesc.vertex.constantCount = 0;
  pipelineDesc.vertex.constants = nullptr;

  pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
  pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
  pipelineDesc.primitive.cullMode = WGPUCullMode_None;

  // The Fragment State controls how fragment shaders process pixel data,
  // including color blending and format.
  //  Use cases
  //  - Transparency and Alpha Blending
  //  - Post-Processing Effects

  WGPUFragmentState fragmentState = {};
  fragmentState.module = shader;
  fragmentState.entryPoint = "fs_main";
  fragmentState.constantCount = 0;
  fragmentState.constants = NULL;

  // The Blend State defines how colors from the fragment shader are blended
  // with the existing color in the framebuffer.
  //  Use cases
  //  - Particle Systems
  //  - Overlaying UI Elements

  WGPUBlendState blendState = {};
  blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
  blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
  blendState.color.operation = WGPUBlendOperation_Add;
  blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
  blendState.alpha.dstFactor = WGPUBlendFactor_One;
  blendState.alpha.operation = WGPUBlendOperation_Add;

  // It specifies the format of the color attachment and how colors are written
  // to it.
  //  Use cases
  //  - Multisampling for Anti-Aliasing:
  //  - HDDR

  WGPUColorTargetState colorTarget = {};
  colorTarget.format = swapChainFormat;
  colorTarget.blend = &blendState;
  colorTarget.writeMask = WGPUColorWriteMask_All;

  fragmentState.targetCount = 1;
  fragmentState.targets = &colorTarget;

  pipelineDesc.fragment = &fragmentState;

  // The Depth Stencil State defines how the pipeline will handle depth testing
  // and stencil testing. Use cases:
  // - Depth Testing for 3D Scenes
  // - Stencil Operations such as Shadow-Mapping

  if (depthFormat == WGPUTextureFormat_Undefined) {
    pipelineDesc.depthStencil = nullptr;
  } else {
    WGPUDepthStencilState depthStencilState = {};
    depthStencilState.format = depthFormat;
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

  // Interface to it's uniforms
  static std::map<std::string, std::map<int, std::vector<WGPUBindGroupLayoutEntry>>> groupLayouts;

  if (groupLayouts.find(pipelineId) == groupLayouts.end()) {
    groupLayouts[pipelineId] = ParseGroupLayout(groupLayout);
  }

  std::vector<WGPUBindGroupLayout> bindGroupLayouts;

  for (int i = 0; i < groupLayouts[pipelineId].size(); i++) {
    WGPUBindGroupLayoutDescriptor descriptor = {};
    descriptor.label = std::string("bgl_" + pipelineId).c_str();
    descriptor.entryCount = groupLayouts[pipelineId][i].size();
    descriptor.entries = groupLayouts[pipelineId][i].data();

    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(_device, &descriptor);
    bindGroupLayouts.push_back(bindGroupLayout);
  }

  static std::map<std::string, WGPUPipelineLayoutDescriptor> layoutDesc;

  if (layoutDesc.find(pipelineId) == layoutDesc.end()) {
    layoutDesc[pipelineId] = {};
  }

  layoutDesc[pipelineId].bindGroupLayoutCount = bindGroupLayouts.size();
  layoutDesc[pipelineId].bindGroupLayouts = bindGroupLayouts.data();

  static std::map<std::string, WGPUPipelineLayout> pipelineLayout;

  if (pipelineLayout.find(pipelineId) == pipelineLayout.end()) {
    pipelineLayout[pipelineId] = wgpuDeviceCreatePipelineLayout(_device, &layoutDesc[pipelineId]);
  }

  pipelineDesc.layout = pipelineLayout[pipelineId];

  WGPURenderPipeline pipe =
      wgpuDeviceCreateRenderPipeline(_device, &pipelineDesc);

  pipelines_.emplace(pipelineId, pipe);
  return pipe;
}

WGPURenderPipeline PipelineManager::GetPipeline(const std::string& pipelineId) {
  return pipelines_[pipelineId];
}
