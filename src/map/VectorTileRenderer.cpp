#include "map/VectorTileRenderer.h"
#include "map/MBTiles.h"
#include "map/VectorTile.h"
#include "core/Log.h"
#include "render/RenderContext.h"
#include "render/RenderUtils.h"
#include "render/ShaderManager.h"

namespace WebEngine
{
  struct TileUniforms
  {
    glm::mat4 viewProjectionMatrix;
  };

  static const char* TILE_SHADER = R"(
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

  // Shader variant for desktop (2 color targets)
  static const char* TILE_SHADER_DUAL = R"(
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

    struct FragmentOutput {
      @location(0) color0: vec4<f32>,
      @location(1) color1: vec4<f32>,
    }

    @fragment
    fn fs_main(input: FragmentInput) -> FragmentOutput {
      var output: FragmentOutput;
      output.color0 = input.color;
      output.color1 = input.color;
      return output;
    }
  )";

  glm::vec4 VectorTileRenderer::GetLayerColor(const std::string& layerName)
  {
    if (layerName == "water") return {0.2f, 0.4f, 0.8f, 1.0f};
    if (layerName == "waterway") return {0.3f, 0.5f, 0.9f, 1.0f};
    if (layerName == "transportation" || layerName == "road") return {0.7f, 0.7f, 0.7f, 1.0f};
    if (layerName == "building") return {0.9f, 0.5f, 0.2f, 1.0f};
    if (layerName == "landuse" || layerName == "landcover") return {0.3f, 0.7f, 0.3f, 1.0f};
    if (layerName == "boundary" || layerName == "admin") return {0.7f, 0.3f, 0.7f, 1.0f};
    if (layerName == "place") return {1.0f, 1.0f, 0.3f, 1.0f};
    if (layerName == "park") return {0.2f, 0.6f, 0.2f, 1.0f};
    return {0.8f, 0.8f, 0.8f, 1.0f};
  }

  void VectorTileRenderer::Init(Ref<Framebuffer> targetFramebuffer)
  {
    CreatePipeline(targetFramebuffer);
  }

  void VectorTileRenderer::CreatePipeline(Ref<Framebuffer> targetFramebuffer)
  {
    const auto device = RenderContext::GetDevice();
    const auto& fboSpec = targetFramebuffer->m_FrameBufferSpec;
    int colorTargetCount = (int)fboSpec.ColorFormats.size();

    // Choose shader based on color target count
    const char* shaderSource = (colorTargetCount >= 2) ? TILE_SHADER_DUAL : TILE_SHADER;
    m_Shader = ShaderManager::LoadShaderFromString("VectorTileShader", shaderSource);

    // Pipeline layout
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.label = RenderUtils::MakeLabel("VectorTile Pipeline Layout");
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &m_Shader->GetLayout(0);
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

    // Vertex attributes: position (vec3) + color (vec4)
    WGPUVertexAttribute attributes[2] = {};
    attributes[0].format = WGPUVertexFormat_Float32x3;
    attributes[0].offset = 0;
    attributes[0].shaderLocation = 0;
    attributes[1].format = WGPUVertexFormat_Float32x4;
    attributes[1].offset = sizeof(glm::vec3);
    attributes[1].shaderLocation = 1;

    WGPUVertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.arrayStride = sizeof(TileVertex);
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;
    vertexBufferLayout.attributeCount = 2;
    vertexBufferLayout.attributes = attributes;

    // Render pipeline descriptor
    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.label = RenderUtils::MakeLabel("VectorTile Render Pipeline");
    pipelineDesc.layout = pipelineLayout;
    pipelineDesc.vertex.module = m_Shader->GetNativeShaderModule();
    pipelineDesc.vertex.entryPoint = RenderUtils::MakeLabel("vs_main");
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;

    // Color targets matching composite framebuffer
    std::vector<WGPUColorTargetState> colorTargets(colorTargetCount);
    for (int i = 0; i < colorTargetCount; i++)
    {
      colorTargets[i].format = RenderTypeUtils::ToRenderType(fboSpec.ColorFormats[i]);
      colorTargets[i].blend = nullptr;
      colorTargets[i].writeMask = WGPUColorWriteMask_All;
    }

    WGPUFragmentState fragmentState = {};
    fragmentState.module = m_Shader->GetNativeShaderModule();
    fragmentState.entryPoint = RenderUtils::MakeLabel("fs_main");
    fragmentState.targetCount = colorTargetCount;
    fragmentState.targets = colorTargets.data();
    pipelineDesc.fragment = &fragmentState;

    // Depth stencil
    WGPUDepthStencilState depthStencilState = {};
    depthStencilState.format = WGPUTextureFormat_Depth24Plus;
    depthStencilState.stencilReadMask = 0xFFFFFFFF;
    depthStencilState.stencilWriteMask = 0xFFFFFFFF;
    depthStencilState.depthCompare = WGPUCompareFunction_Less;
#ifndef __EMSCRIPTEN__
    depthStencilState.depthWriteEnabled = WGPUOptionalBool_True;
#else
    depthStencilState.depthWriteEnabled = true;
#endif
    depthStencilState.stencilFront.compare = WGPUCompareFunction_Always;
    depthStencilState.stencilFront.failOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilFront.depthFailOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilFront.passOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilBack = depthStencilState.stencilFront;
    pipelineDesc.depthStencil = &depthStencilState;

    // Primitive: line list
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_LineList;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;

    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    m_Pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
    wgpuPipelineLayoutRelease(pipelineLayout);

    // Uniform buffer
    m_UniformBuffer = GPUAllocator::GAlloc("VectorTile Uniforms",
                                           WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
                                           sizeof(TileUniforms));

    // Bind group
    WGPUBindGroupEntry bindGroupEntry = {};
    bindGroupEntry.binding = 0;
    bindGroupEntry.buffer = m_UniformBuffer->Buffer;
    bindGroupEntry.offset = 0;
    bindGroupEntry.size = sizeof(TileUniforms);

    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.label = RenderUtils::MakeLabel("VectorTile Bind Group");
    bindGroupDesc.layout = m_Shader->GetLayout(0);
    bindGroupDesc.entryCount = 1;
    bindGroupDesc.entries = &bindGroupEntry;
    m_BindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);

    RN_LOG("VectorTileRenderer pipeline created ({} color targets)", colorTargetCount);
  }

  static constexpr float TILE_WORLD_SIZE = 10.0f;

  void VectorTileRenderer::LoadTileRect(const MBTilesReader& source, int zoom,
                                        int minTX, int minTY, int maxTX, int maxTY,
                                        int refTX, int refTY)
  {
    if (!source.IsOpen())
    {
      RN_LOG_ERR("VectorTileRenderer: tile source is not open");
      return;
    }

    // Clamp the requested rect to the valid tile range for this zoom so we
    // don't waste sqlite queries on coordinates outside the pyramid.
    const int maxIndex = (1 << zoom) - 1;
    minTX = std::max(minTX, 0);
    minTY = std::max(minTY, 0);
    maxTX = std::min(maxTX, maxIndex);
    maxTY = std::min(maxTY, maxIndex);

    if (minTX > maxTX || minTY > maxTY)
    {
      RN_LOG_ERR("VectorTileRenderer: empty tile rect at zoom {}", zoom);
      m_VertexCount = 0;
      m_Ready = false;
      return;
    }

    std::vector<TileVertex> vertices;
    int tilesLoaded = 0;
    int tilesQueried = 0;

    for (int tx = minTX; tx <= maxTX; tx++)
    {
      for (int ty = minTY; ty <= maxTY; ty++)
      {
        tilesQueried++;
        auto bytes = source.ReadTile(zoom, tx, ty);
        if (bytes.empty())
          continue;

        MVTTile tile = ParseMVTFromBytes(bytes.data(), bytes.size());
        if (tile.layers.empty())
          continue;

        int dx = tx - refTX;
        int dy = ty - refTY;
        glm::vec2 offset = {
            (float)dx * TILE_WORLD_SIZE,
            (float)(-dy) * TILE_WORLD_SIZE
        };

        AppendTileGeometry(tile, offset, vertices);
        tilesLoaded++;
      }
    }

    if (tilesLoaded == 0)
    {
      RN_LOG_ERR("VectorTileRenderer: No tiles found in rect z={} x=[{}..{}] y=[{}..{}] ({} queried)",
                 zoom, minTX, maxTX, minTY, maxTY, tilesQueried);
      m_VertexCount = 0;
      m_Ready = false;
      return;
    }

    UploadGeometry(vertices);
    m_Ready = true;
    RN_LOG("VectorTileRenderer: Loaded {}/{} tiles at zoom {} rect x=[{}..{}] y=[{}..{}]",
           tilesLoaded, tilesQueried, zoom, minTX, maxTX, minTY, maxTY);
  }

  void VectorTileRenderer::AppendTileGeometry(const MVTTile& tile, glm::vec2 worldOffset, std::vector<TileVertex>& vertices)
  {
    // Add tile border (bright yellow rectangle)
    float half = TILE_WORLD_SIZE * 0.5f;
    glm::vec4 borderColor = {1.0f, 1.0f, 0.0f, 1.0f};
    glm::vec3 corners[4] = {
        {worldOffset.x - half, 0.1f, worldOffset.y - half},
        {worldOffset.x + half, 0.1f, worldOffset.y - half},
        {worldOffset.x + half, 0.1f, worldOffset.y + half},
        {worldOffset.x - half, 0.1f, worldOffset.y + half},
    };
    for (int i = 0; i < 4; i++)
    {
      vertices.push_back({corners[i], borderColor});
      vertices.push_back({corners[(i + 1) % 4], borderColor});
    }

    for (const auto& layer : tile.layers)
    {
      glm::vec4 color = GetLayerColor(layer.name);
      float extent = (float)layer.extent;
      float scale = TILE_WORLD_SIZE / extent;

      for (const auto& feature : layer.features)
      {
        if (feature.type == MVTGeomType::Point)
          continue;

        for (const auto& ring : feature.rings)
        {
          for (size_t i = 0; i + 1 < ring.size(); i++)
          {
            glm::vec3 p0 = {
                (ring[i].x - extent * 0.5f) * scale + worldOffset.x,
                0.0f,
                (extent * 0.5f - ring[i].y) * scale + worldOffset.y};

            glm::vec3 p1 = {
                (ring[i + 1].x - extent * 0.5f) * scale + worldOffset.x,
                0.0f,
                (extent * 0.5f - ring[i + 1].y) * scale + worldOffset.y};

            vertices.push_back({p0, color});
            vertices.push_back({p1, color});
          }
        }
      }
    }
  }

  void VectorTileRenderer::UploadGeometry(const std::vector<TileVertex>& vertices)
  {
    m_VertexCount = (uint32_t)vertices.size();
    if (m_VertexCount == 0)
    {
      RN_LOG("VectorTileRenderer: No geometry generated");
      return;
    }

    size_t dataSize = vertices.size() * sizeof(TileVertex);
    m_VertexBuffer = GPUAllocator::GAlloc("VectorTile Vertices",
                                          WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
                                          (int)dataSize);
    m_VertexBuffer->SetData(vertices.data(), (int)dataSize);

    RN_LOG("VectorTileRenderer: {} vertices ({} line segments), {:.1f} KB",
             m_VertexCount, m_VertexCount / 2, dataSize / 1024.0f);
  }

  void VectorTileRenderer::Render(WGPURenderPassEncoder passEncoder, const glm::mat4& viewProjection)
  {
    if (!m_Ready || m_VertexCount == 0)
      return;

    TileUniforms uniforms = {};
    uniforms.viewProjectionMatrix = viewProjection;
    m_UniformBuffer->SetData(&uniforms, sizeof(uniforms));

    size_t vertexDataSize = m_VertexCount * sizeof(TileVertex);

    wgpuRenderPassEncoderSetPipeline(passEncoder, m_Pipeline);
    wgpuRenderPassEncoderSetBindGroup(passEncoder, 0, m_BindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(passEncoder, 0, m_VertexBuffer->Buffer, 0, vertexDataSize);
    wgpuRenderPassEncoderDraw(passEncoder, m_VertexCount, 1, 0, 0);
  }
}
