#include "map/VectorTileRenderer.h"
#include <string>
#include "map/MBTiles.h"
#include "map/MapProjection.h"
#include "map/VectorTile.h"
#include "core/Log.h"
#include "render/RenderContext.h"
#include "render/RenderUtils.h"
#include "render/ShaderManager.h"

namespace WebEngine
{
  // ---------------------------------------------------------------------------
  // Shader sources
  //
  // The vertex stage and fragment input are identical for every variant — only
  // the fragment *output* changes based on how many color attachments the
  // target framebuffer has. BuildShaderSource() glues the common part with the
  // right fragment tail instead of maintaining two near-identical shaders.
  // ---------------------------------------------------------------------------

  namespace
  {
    static const char* SHADER_COMMON = R"(
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
    )";

    static const char* SHADER_FRAGMENT_SINGLE = R"(
      @fragment
      fn fs_main(input: FragmentInput) -> @location(0) vec4<f32> {
        return input.color;
      }
    )";

    // Desktop composite framebuffer has two color attachments (lit + brightness).
    static const char* SHADER_FRAGMENT_DUAL = R"(
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

    std::string BuildShaderSource(int colorTargetCount)
    {
      std::string src = SHADER_COMMON;
      src += (colorTargetCount >= 2) ? SHADER_FRAGMENT_DUAL : SHADER_FRAGMENT_SINGLE;
      return src;
    }

    // Must match the Uniforms struct in WGSL above.
    struct TileUniforms
    {
      glm::mat4 viewProjectionMatrix;
    };

    // ---------------------------------------------------------------------------
    // Geometry helpers
    // ---------------------------------------------------------------------------

    constexpr float BORDER_HEIGHT = 0.1f;
    const glm::vec4 BORDER_COLOR = {1.0f, 1.0f, 0.0f, 1.0f};

    glm::vec4 LookupLayerColor(const std::string& name)
    {
      if (name == "water")                            return {0.2f, 0.4f, 0.8f, 1.0f};
      if (name == "waterway")                         return {0.3f, 0.5f, 0.9f, 1.0f};
      if (name == "transportation" || name == "road") return {0.7f, 0.7f, 0.7f, 1.0f};
      if (name == "building")                         return {0.9f, 0.5f, 0.2f, 1.0f};
      if (name == "landuse" || name == "landcover")   return {0.3f, 0.7f, 0.3f, 1.0f};
      if (name == "boundary" || name == "admin")      return {0.7f, 0.3f, 0.7f, 1.0f};
      if (name == "place")                            return {1.0f, 1.0f, 0.3f, 1.0f};
      if (name == "park")                             return {0.2f, 0.6f, 0.2f, 1.0f};
      return {0.8f, 0.8f, 0.8f, 1.0f};
    }

    void AppendLine(std::vector<TileVertex>& verts,
                    const glm::vec3& a, const glm::vec3& b, const glm::vec4& color)
    {
      verts.push_back({a, color});
      verts.push_back({b, color});
    }

    // Yellow square matching the tile footprint — keeps empty tiles visible
    // and makes tile seams obvious at a glance.
    void AppendTileBorder(std::vector<TileVertex>& verts, glm::vec2 worldOffset)
    {
      const float half = MapProjection::TILE_WORLD_SIZE * 0.5f;
      const glm::vec3 corners[4] = {
          {worldOffset.x - half, BORDER_HEIGHT, worldOffset.y - half},
          {worldOffset.x + half, BORDER_HEIGHT, worldOffset.y - half},
          {worldOffset.x + half, BORDER_HEIGHT, worldOffset.y + half},
          {worldOffset.x - half, BORDER_HEIGHT, worldOffset.y + half},
      };
      for (int i = 0; i < 4; i++)
      {
        AppendLine(verts, corners[i], corners[(i + 1) % 4], BORDER_COLOR);
      }
    }

    // MVT ring coordinates are integers in [0, extent]. Convert to world space,
    // centered on the tile, with world +X=east and +Z=north. MVT Y grows south
    // so we flip it here.
    glm::vec3 TileLocalToWorld(glm::vec2 local, float extent, glm::vec2 worldOffset)
    {
      const float scale = MapProjection::TILE_WORLD_SIZE / extent;
      return {
          (local.x - extent * 0.5f) * scale + worldOffset.x,
          0.0f,
          (extent * 0.5f - local.y) * scale + worldOffset.y};
    }

    void AppendTileFeatures(std::vector<TileVertex>& verts,
                            const MVTTile& tile, glm::vec2 worldOffset)
    {
      for (const auto& layer : tile.layers)
      {
        const glm::vec4 color = LookupLayerColor(layer.name);
        const float extent = (float)layer.extent;

        for (const auto& feature : layer.features)
        {
          // Points have nothing to draw as line segments.
          if (feature.type == MVTGeomType::Point)
            continue;

          for (const auto& ring : feature.rings)
          {
            for (size_t i = 0; i + 1 < ring.size(); i++)
            {
              const glm::vec3 a = TileLocalToWorld(ring[i],     extent, worldOffset);
              const glm::vec3 b = TileLocalToWorld(ring[i + 1], extent, worldOffset);
              AppendLine(verts, a, b, color);
            }
          }
        }
      }
    }
  }  // namespace

  // ---------------------------------------------------------------------------
  // VectorTileRenderer
  // ---------------------------------------------------------------------------

  void VectorTileRenderer::Init(const Ref<Framebuffer>& targetFramebuffer)
  {
    CreatePipeline(targetFramebuffer);
  }

  void VectorTileRenderer::CreatePipeline(const Ref<Framebuffer>& targetFramebuffer)
  {
    const auto device = RenderContext::GetDevice();
    const auto& fboSpec = targetFramebuffer->m_FrameBufferSpec;
    const int colorTargetCount = (int)fboSpec.ColorFormats.size();

    // 1. Shader — picks the fragment variant that matches the target FBO.
    m_Shader = ShaderManager::LoadShaderFromString(
        "VectorTileShader", BuildShaderSource(colorTargetCount));

    // 2. Vertex buffer layout — interleaved position (vec3) + color (vec4).
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

    // 3. Color targets — one per FBO color attachment, no blending.
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

    // 4. Depth/stencil — depth test against the scene depth so 3D objects
    //    above y=0 can occlude the tiles.
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

    // 5. Pipeline layout comes from the shader's bind group 0 (uniform buffer).
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.label = RenderUtils::MakeLabel("VectorTile Pipeline Layout");
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &m_Shader->GetLayout(0);
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

    // 6. Render pipeline.
    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.label = RenderUtils::MakeLabel("VectorTile Render Pipeline");
    pipelineDesc.layout = pipelineLayout;
    pipelineDesc.vertex.module = m_Shader->GetNativeShaderModule();
    pipelineDesc.vertex.entryPoint = RenderUtils::MakeLabel("vs_main");
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;
    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.depthStencil = &depthStencilState;
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_LineList;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    m_Pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
    wgpuPipelineLayoutRelease(pipelineLayout);

    // 7. Uniform buffer + bind group (single binding: the uniforms).
    m_UniformBuffer = GPUAllocator::GAlloc("VectorTile Uniforms",
                                           WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
                                           sizeof(TileUniforms));

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

  void VectorTileRenderer::LoadTileRect(const MBTilesReader& source, int zoom,
                                        int minTX, int minTY, int maxTX, int maxTY,
                                        int refTX, int refTY)
  {
    if (!source.IsOpen())
    {
      RN_LOG_ERR("VectorTileRenderer: tile source is not open");
      return;
    }

    // Clamp to the valid tile range for this zoom so we don't waste sqlite
    // queries on coordinates outside the pyramid.
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

        // Tile (tx, ty) sits at (tx - refTX, -(ty - refTY)) tiles from world
        // origin — Y is negated because MVT y grows southward.
        const glm::vec2 offset = {
            (float)(tx - refTX) * MapProjection::TILE_WORLD_SIZE,
            (float)(refTY - ty) * MapProjection::TILE_WORLD_SIZE};

        AppendTileBorder(vertices, offset);
        AppendTileFeatures(vertices, tile, offset);
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

    UploadVertices(vertices);
    m_Ready = true;
    RN_LOG("VectorTileRenderer: Loaded {}/{} tiles at zoom {} rect x=[{}..{}] y=[{}..{}]",
           tilesLoaded, tilesQueried, zoom, minTX, maxTX, minTY, maxTY);
  }

  void VectorTileRenderer::UploadVertices(const std::vector<TileVertex>& vertices)
  {
    m_VertexCount = (uint32_t)vertices.size();
    if (m_VertexCount == 0)
    {
      RN_LOG("VectorTileRenderer: No geometry generated");
      return;
    }

    const size_t dataSize = vertices.size() * sizeof(TileVertex);
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

    const size_t vertexDataSize = m_VertexCount * sizeof(TileVertex);
    wgpuRenderPassEncoderSetPipeline(passEncoder, m_Pipeline);
    wgpuRenderPassEncoderSetBindGroup(passEncoder, 0, m_BindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(passEncoder, 0, m_VertexBuffer->Buffer, 0, vertexDataSize);
    wgpuRenderPassEncoderDraw(passEncoder, m_VertexCount, 1, 0, 0);
  }
}  // namespace WebEngine
