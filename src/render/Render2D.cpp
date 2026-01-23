#include "Render2D.h"
#include "physics/PhysicUtils.h"
#include "render/RenderUtils.h"
#include "render/ShaderManager.h"

namespace Rain
{
  std::unique_ptr<RenderDebug> RenderDebug::Instance;

  void RenderDebug::Begin(WGPURenderPassEncoder* pass, WGPUCommandEncoder* encoder)
  {
    Instance->m_TargetEncoder = encoder;
    Instance->m_TargetPass = pass;
  }

  void RenderDebug::Init()
  {
    Instance = std::make_unique<RenderDebug>();
    Instance->InitializeResources();
  }

  void RenderDebug::InitializeResources()
  {
    const char* LINE_SHADER = R"(
		struct Uniforms {
		    viewProjectionMatrix: mat4x4<f32>,
		}
		
		struct VertexInput {
		    @location(0) position: vec3<f32>,
		    @location(1) color: vec4<f32>,
		}
		
		struct VertexOutput {
		    @builtin(position) position: vec4<f32>,
		    @location(0) color: vec4<f32>,
		}
		
		@group(0) @binding(0) var<uniform> uniforms: Uniforms;
		
		@vertex
		fn vs_main(input: VertexInput) -> VertexOutput {
		    var output: VertexOutput;
		    output.position = uniforms.viewProjectionMatrix * vec4<f32>(input.position, 1.0);
		    output.color = input.color;
		    return output;
		}
		struct FragmentInput {
		    @location(0) color: vec4<f32>,
		}
		
		@fragment
		fn fs_main(input: FragmentInput) -> @location(0) vec4<f32> {
		    return input.color;
		}
		)";

    auto device = RenderContext::GetDevice();

    m_LineShader = ShaderManager::LoadShaderFromString("LineShader", LINE_SHADER);

    WGPUBindGroupLayoutEntry bindGroupLayoutEntry = {};
    bindGroupLayoutEntry.binding = 0;
    bindGroupLayoutEntry.visibility = WGPUShaderStage_Vertex;
    bindGroupLayoutEntry.buffer.type = WGPUBufferBindingType_Uniform;
    bindGroupLayoutEntry.buffer.minBindingSize = sizeof(LineUniforms);

    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.label = RenderUtils::MakeLabel("Line Pipeline Layout");
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &m_LineShader->GetLayout(0);
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

    WGPUVertexAttribute attributes[2] = {};
    attributes[0].format = WGPUVertexFormat_Float32x3;
    attributes[0].offset = 0;
    attributes[0].shaderLocation = 0;

    attributes[1].format = WGPUVertexFormat_Float32x4;
    attributes[1].offset = sizeof(glm::vec3);
    attributes[1].shaderLocation = 1;

    WGPUVertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.arrayStride = sizeof(LineVertex);
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;
    vertexBufferLayout.attributeCount = 2;
    vertexBufferLayout.attributes = attributes;

    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.label = RenderUtils::MakeLabel("Line Render Pipeline");
    pipelineDesc.layout = pipelineLayout;

    WGPUDepthStencilState* depthStencilState = ZERO_ALLOC(WGPUDepthStencilState);
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
#ifndef __EMSCRIPTEN__
    depthStencilState->depthWriteEnabled = WGPUOptionalBool_True;
#endif

    depthStencilState->stencilReadMask = 0xFFFFFFFF;
    depthStencilState->stencilWriteMask = 0xFFFFFFFF;
    depthStencilState->nextInChain = nullptr;

    pipelineDesc.depthStencil = depthStencilState;

    // Vertex state
    pipelineDesc.vertex.module = m_LineShader->GetNativeShaderModule();
    pipelineDesc.vertex.entryPoint = RenderUtils::MakeLabel("vs_main");
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;

    // Fragment state
    WGPUFragmentState fragmentState = {};
    fragmentState.module = m_LineShader->GetNativeShaderModule();
    fragmentState.entryPoint = RenderUtils::MakeLabel("fs_main");

    WGPUColorTargetState colorTarget = {};
    colorTarget.format = WGPUTextureFormat_BGRA8Unorm;
    colorTarget.blend = nullptr;  // No blending for now
    colorTarget.writeMask = WGPUColorWriteMask_All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    pipelineDesc.fragment = &fragmentState;

    // Primitive state
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_LineList;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;

    // Multisample state
    pipelineDesc.multisample.count = 4;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    m_LinePipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

    m_LineUniformBuffer = GPUAllocator::GAlloc("Line Uniforms",
                                               WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
                                               sizeof(LineUniforms));

    // Create bind group
    WGPUBindGroupEntry bindGroupEntry = {};
    bindGroupEntry.binding = 0;
    bindGroupEntry.buffer = m_LineUniformBuffer->Buffer;
    bindGroupEntry.offset = 0;
    bindGroupEntry.size = sizeof(LineUniforms);

    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.label = RenderUtils::MakeLabel("Line Bind Group");
    bindGroupDesc.layout = m_LineShader->GetLayout(0);
    bindGroupDesc.entryCount = 1;
    bindGroupDesc.entries = &bindGroupEntry;
    m_LineBindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);

    // Cleanup shader modules
    wgpuPipelineLayoutRelease(pipelineLayout);
  }

  void RenderDebug::Shutdown()
  {
  }

  void RenderDebug::DrawCircle(const glm::vec3& p0, const glm::vec3& rotation, float radius, const glm::vec4& color)
  {
  }

  void RenderDebug::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color)
  {
    Instance->m_QueuedLines.push_back({p0, p1, color});
  }

  void RenderDebug::FlushDrawList()
  {
    if (Instance->m_QueuedLines.empty())
    {
      return;
    }

    size_t totalVertices = Instance->m_QueuedLines.size() * 2;
    size_t vertexDataSize = totalVertices * sizeof(LineVertex);

    if (!Instance->m_LineVertexBuffer || Instance->m_LineVertexBuffer->Size < vertexDataSize)
    {
      size_t bufferSize = std::max(vertexDataSize, sizeof(LineVertex) * MAX_LINES_PER_BATCH * 2);
      Instance->m_LineVertexBuffer = GPUAllocator::GAlloc("Line Vertices",
                                                          WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
                                                          bufferSize);
    }

    std::vector<LineVertex> vertices;
    vertices.reserve(totalVertices);

    for (const auto& line : Instance->m_QueuedLines)
    {
      vertices.push_back({line.p0, line.color});
      vertices.push_back({line.p1, line.color});
    }

    Instance->m_LineVertexBuffer->SetData(vertices.data(), vertexDataSize);

    LineUniforms uniforms = {};
    uniforms.viewProjectionMatrix = Instance->m_MVP;
    Instance->m_LineUniformBuffer->SetData(&uniforms, sizeof(uniforms));

    wgpuRenderPassEncoderSetPipeline(*Instance->m_TargetPass, Instance->m_LinePipeline);
    wgpuRenderPassEncoderSetBindGroup(*Instance->m_TargetPass, 0, Instance->m_LineBindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(*Instance->m_TargetPass, 0, Instance->m_LineVertexBuffer->Buffer, 0, vertexDataSize);
    wgpuRenderPassEncoderDraw(*Instance->m_TargetPass, static_cast<uint32_t>(totalVertices), 1, 0, 0);

    Instance->m_QueuedLines.clear();
  }

  void RenderDebug::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
  {
    glm::vec3 from = PhysicsUtils::FromJoltVector(inFrom);
    glm::vec3 to = PhysicsUtils::FromJoltVector(inTo);
    glm::vec4 color = glm::vec4(inColor.r, inColor.g, inColor.b, 1.0f);

    DrawLine(from, to, color);
  }

  void RenderDebug::DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor, float inHeight)
  {
    // TODO: Implement 3D text rendering if needed
    // For now, this is a stub to make the class non-abstract
    // You can leave this empty or add basic text rendering later
  }

  void RenderDebug::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow)
  {
    glm::vec3 v1 = PhysicsUtils::FromJoltVector(inV1);
    glm::vec3 v2 = PhysicsUtils::FromJoltVector(inV2);
    glm::vec3 v3 = PhysicsUtils::FromJoltVector(inV3);
    glm::vec4 color = glm::vec4(1.0, 0.0, 0.0, 1.0);

    DrawLine(v1, v2, color);
    DrawLine(v2, v3, color);
    DrawLine(v3, v1, color);
  }
}  // namespace Rain
