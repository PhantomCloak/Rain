#include "map/VectorTileRenderer.h"
#include <string>
#include "map/MBTiles.h"
#include "map/MapProjection.h"
#include "map/VectorTile.h"
#include "core/Log.h"
#include "render/RenderContext.h"
#include "render/RenderUtils.h"
#include "render/ShaderManager.h"

// earcut.hpp declares the primary template `mapbox::util::nth<I, T>`. We
// include it first, then add glm::vec2 specializations at that exact
// namespace scope — earcut calls `mapbox::util::nth<...>::get(p)` by fully
// qualified name, so the specializations must live in mapbox::util, not in
// WebEngine.
#include <mapbox/earcut.hpp>

namespace mapbox
{
  namespace util
  {
    template <>
    struct nth<0, glm::vec2>
    {
      static float get(const glm::vec2& p) { return p.x; }
    };
    template <>
    struct nth<1, glm::vec2>
    {
      static float get(const glm::vec2& p) { return p.y; }
    };
  }  // namespace util
}  // namespace mapbox

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

    // Shoelace / surveyor's formula over a closed MVT ring. MVT tile-local
    // coordinates are y-down (screen convention), where the spec says
    // signed_area > 0 → exterior ring, < 0 → interior (hole). Crucially this
    // must run on the raw tile-local coords before TileLocalToWorld flips y,
    // because that flip inverts the sign of the result.
    //
    // Accumulator is double to keep long thin rings from cancelling into noise.
    // ClosePath duplicates ring.front() onto ring.back(), so walk [0, n-1)
    // and close the loop modulo-style to avoid double-counting that vertex.
    double SignedAreaTileLocal(const std::vector<glm::vec2>& ring)
    {
      const size_t n = ring.size();
      if (n < 4)
        return 0.0;

      const size_t m = n - 1;  // skip duplicated closing vertex
      double sum = 0.0;
      for (size_t i = 0; i < m; i++)
      {
        const glm::vec2& a = ring[i];
        const glm::vec2& b = ring[(i + 1) % m];
        sum += (double)a.x * (double)b.y - (double)b.x * (double)a.y;
      }
      return 0.5 * sum;
    }

    // A single contiguous polygon: one outer ring followed by zero or more
    // hole rings. Stored as pointers into the source MVTFeature::rings so we
    // never copy the underlying vec2 data.
    struct GroupedPolygon
    {
      const std::vector<glm::vec2>* outer = nullptr;
      std::vector<const std::vector<glm::vec2>*> holes;
    };

    // Walk rings in source order and group them using the shoelace sign rule.
    // The MVT spec guarantees interior rings appear after their containing
    // exterior ring, so a single left-to-right pass is sufficient — no spatial
    // tests needed. Rings with fewer than 4 points are degenerate and skipped.
    // Holes that appear before any exterior ring are malformed and dropped.
    std::vector<GroupedPolygon> GroupRings(const MVTFeature& feature)
    {
      std::vector<GroupedPolygon> polys;
      for (const auto& ring : feature.rings)
      {
        if (ring.size() < 4)
          continue;

        const double area = SignedAreaTileLocal(ring);
        if (area > 0.0)
        {
          GroupedPolygon g;
          g.outer = &ring;
          polys.push_back(std::move(g));
        }
        else if (area < 0.0 && !polys.empty())
        {
          polys.back().holes.push_back(&ring);
        }
        // area == 0 → collinear / degenerate, skip silently
      }
      return polys;
    }

    // Triangulate one grouped polygon via earcut and append the result to
    // (fillVerts, fillIndices). Points are transformed to world space at
    // insertion time. The earcut input is built without the duplicated
    // closing vertex on each ring (earcut treats rings as implicitly closed;
    // feeding it the duplicate creates degenerate triangles and can trip its
    // internal asserts in debug builds).
    void TriangulateAndAppend(const GroupedPolygon& poly,
                              float extent,
                              glm::vec2 worldOffset,
                              const glm::vec4& color,
                              std::vector<TileVertex>& fillVerts,
                              std::vector<uint32_t>& fillIndices)
    {
      // Build earcut input: outer, then each hole, each with closing dup stripped.
      std::vector<std::vector<glm::vec2>> earcutInput;
      earcutInput.reserve(1 + poly.holes.size());
      earcutInput.emplace_back(poly.outer->begin(), poly.outer->end() - 1);
      for (const auto* hole : poly.holes)
      {
        earcutInput.emplace_back(hole->begin(), hole->end() - 1);
      }

      const std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(earcutInput);
      if (indices.empty() || (indices.size() % 3) != 0)
      {
        static bool warned = false;
        if (!warned)
        {
          RN_LOG_ERR("VectorTileRenderer: earcut produced {} indices (not a multiple of 3) — skipping polygon; further failures suppressed",
                     indices.size());
          warned = true;
        }
        return;
      }

      // Append vertices in the same order as earcutInput so indices line up.
      const uint32_t baseVertex = (uint32_t)fillVerts.size();
      for (const auto& ring : earcutInput)
      {
        for (const auto& p : ring)
        {
          fillVerts.push_back({TileLocalToWorld(p, extent, worldOffset), color});
        }
      }

      fillIndices.reserve(fillIndices.size() + indices.size());
      for (const uint32_t idx : indices)
      {
        fillIndices.push_back(baseVertex + idx);
      }
    }

    // Walk every feature in every layer and emit it into the appropriate
    // output buffer: MVT Polygon features are ring-classified and triangulated
    // into (fillVerts, fillIndices); LineString features become line segments
    // in lineVerts. Point features are skipped.
    void AppendTileFeatures(std::vector<TileVertex>& lineVerts,
                            std::vector<TileVertex>& fillVerts,
                            std::vector<uint32_t>& fillIndices,
                            const MVTTile& tile, glm::vec2 worldOffset)
    {
      for (const auto& layer : tile.layers)
      {
        const glm::vec4 color = LookupLayerColor(layer.name);
        const float extent = (float)layer.extent;

        for (const auto& feature : layer.features)
        {
          if (feature.type == MVTGeomType::Point)
            continue;

          if (feature.type == MVTGeomType::Polygon)
          {
            const auto polys = GroupRings(feature);
            for (const auto& poly : polys)
            {
              TriangulateAndAppend(poly, extent, worldOffset, color,
                                   fillVerts, fillIndices);
            }
            continue;
          }

          // LineString (and anything else unknown) → line segments.
          for (const auto& ring : feature.rings)
          {
            for (size_t i = 0; i + 1 < ring.size(); i++)
            {
              const glm::vec3 a = TileLocalToWorld(ring[i],     extent, worldOffset);
              const glm::vec3 b = TileLocalToWorld(ring[i + 1], extent, worldOffset);
              AppendLine(lineVerts, a, b, color);
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
    CreatePipelines(targetFramebuffer);
  }

  void VectorTileRenderer::CreatePipelines(const Ref<Framebuffer>& targetFramebuffer)
  {
    const auto device = RenderContext::GetDevice();
    const auto& fboSpec = targetFramebuffer->m_FrameBufferSpec;
    const int colorTargetCount = (int)fboSpec.ColorFormats.size();

    // 1. Shader — picks the fragment variant that matches the target FBO.
    m_Shader = ShaderManager::LoadShaderFromString(
        "VectorTileShader", BuildShaderSource(colorTargetCount));

    // 2. Vertex buffer layout — interleaved position (vec3) + color (vec4).
    //    Shared by both pipelines.
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

    // 3. Color targets — one per FBO color attachment, no blending. Shared.
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

    // 4. Depth state templates. Fill writes depth (Less + write); lines use
    //    LessEqual with depth write OFF so they sit on top of fills at the
    //    same y with no z-fighting. See docs/vector-tile-parsing-and-rendering.md.
    auto makeDepthState = [](WGPUCompareFunction compare, bool writeEnabled) {
      WGPUDepthStencilState s = {};
      s.format = WGPUTextureFormat_Depth24Plus;
      s.stencilReadMask = 0xFFFFFFFF;
      s.stencilWriteMask = 0xFFFFFFFF;
      s.depthCompare = compare;
#ifndef __EMSCRIPTEN__
      s.depthWriteEnabled = writeEnabled ? WGPUOptionalBool_True : WGPUOptionalBool_False;
#else
      s.depthWriteEnabled = writeEnabled;
#endif
      s.stencilFront.compare = WGPUCompareFunction_Always;
      s.stencilFront.failOp = WGPUStencilOperation_Keep;
      s.stencilFront.depthFailOp = WGPUStencilOperation_Keep;
      s.stencilFront.passOp = WGPUStencilOperation_Keep;
      s.stencilBack = s.stencilFront;
      return s;
    };

    WGPUDepthStencilState fillDepthState = makeDepthState(WGPUCompareFunction_Less, true);
    WGPUDepthStencilState lineDepthState = makeDepthState(WGPUCompareFunction_LessEqual, false);

    // 5. Pipeline layout comes from the shader's bind group 0 (uniform buffer).
    //    One layout object — both pipelines share it so a single SetBindGroup
    //    before the two draws satisfies WebGPU's bind-group-compatibility rule.
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.label = RenderUtils::MakeLabel("VectorTile Pipeline Layout");
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &m_Shader->GetLayout(0);
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

    // 6. Pipeline descriptor template — same shader/vertex/fragment/layout,
    //    diverging only in primitive topology and depth state per pipeline.
    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.layout = pipelineLayout;
    pipelineDesc.vertex.module = m_Shader->GetNativeShaderModule();
    pipelineDesc.vertex.entryPoint = RenderUtils::MakeLabel("vs_main");
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;
    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    // Fill pipeline: triangles, depth Less + write.
    pipelineDesc.label = RenderUtils::MakeLabel("VectorTile Fill Pipeline");
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.depthStencil = &fillDepthState;
    m_FillPipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

    // Line pipeline: line list, depth LessEqual, write off — overlays fills.
    pipelineDesc.label = RenderUtils::MakeLabel("VectorTile Line Pipeline");
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_LineList;
    pipelineDesc.depthStencil = &lineDepthState;
    m_LinePipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

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

    RN_LOG("VectorTileRenderer pipelines created (fill+line, {} color targets)", colorTargetCount);
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
      m_LineVertexCount = 0;
      m_FillIndexCount = 0;
      m_Ready = false;
      return;
    }

    std::vector<TileVertex> lineVerts;
    std::vector<TileVertex> fillVerts;
    std::vector<uint32_t> fillIndices;
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

        AppendTileBorder(lineVerts, offset);
        AppendTileFeatures(lineVerts, fillVerts, fillIndices, tile, offset);
        tilesLoaded++;
      }
    }

    if (tilesLoaded == 0)
    {
      RN_LOG_ERR("VectorTileRenderer: No tiles found in rect z={} x=[{}..{}] y=[{}..{}] ({} queried)",
                 zoom, minTX, maxTX, minTY, maxTY, tilesQueried);
      m_LineVertexCount = 0;
      m_FillIndexCount = 0;
      m_Ready = false;
      return;
    }

    UploadGeometry(lineVerts, fillVerts, fillIndices);
    m_Ready = true;
    RN_LOG("VectorTileRenderer: Loaded {}/{} tiles at zoom {} rect x=[{}..{}] y=[{}..{}]",
           tilesLoaded, tilesQueried, zoom, minTX, maxTX, minTY, maxTY);
  }

  void VectorTileRenderer::UploadGeometry(const std::vector<TileVertex>& lineVerts,
                                          const std::vector<TileVertex>& fillVerts,
                                          const std::vector<uint32_t>& fillIndices)
  {
    m_LineVertexCount = (uint32_t)lineVerts.size();
    m_FillIndexCount = (uint32_t)fillIndices.size();

    // Line buffer (linestrings + tile borders).
    if (!lineVerts.empty())
    {
      const size_t lineSize = lineVerts.size() * sizeof(TileVertex);
      m_LineVertexBuffer = GPUAllocator::GAlloc("VectorTile Line Vertices",
                                                WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
                                                (int)lineSize);
      m_LineVertexBuffer->SetData(lineVerts.data(), (int)lineSize);
    }
    else
    {
      m_LineVertexBuffer = nullptr;
    }

    // Fill vertex + index buffers (triangulated polygons).
    if (!fillVerts.empty() && !fillIndices.empty())
    {
      const size_t fillVertSize = fillVerts.size() * sizeof(TileVertex);
      m_FillVertexBuffer = GPUAllocator::GAlloc("VectorTile Fill Vertices",
                                                WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
                                                (int)fillVertSize);
      m_FillVertexBuffer->SetData(fillVerts.data(), (int)fillVertSize);

      // uint32 indices are inherently 4-byte aligned; WebGPU requires index
      // buffer size to be a multiple of 4 and this satisfies it naturally.
      const size_t fillIdxSize = fillIndices.size() * sizeof(uint32_t);
      m_FillIndexBuffer = GPUAllocator::GAlloc("VectorTile Fill Indices",
                                               WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst,
                                               (int)fillIdxSize);
      m_FillIndexBuffer->SetData(fillIndices.data(), (int)fillIdxSize);
    }
    else
    {
      m_FillVertexBuffer = nullptr;
      m_FillIndexBuffer = nullptr;
    }

    const double lineKB = (double)(lineVerts.size() * sizeof(TileVertex)) / 1024.0;
    const double fillKB = (double)(fillVerts.size() * sizeof(TileVertex) + fillIndices.size() * sizeof(uint32_t)) / 1024.0;
    RN_LOG("VectorTileRenderer: lines={} verts ({} segments, {:.1f} KB), fills={} tris / {} verts ({:.1f} KB)",
           m_LineVertexCount, m_LineVertexCount / 2, lineKB,
           m_FillIndexCount / 3, (uint32_t)fillVerts.size(), fillKB);
  }

  void VectorTileRenderer::Render(WGPURenderPassEncoder passEncoder, const glm::mat4& viewProjection)
  {
    if (!m_Ready || (m_LineVertexCount == 0 && m_FillIndexCount == 0))
      return;

    TileUniforms uniforms = {};
    uniforms.viewProjectionMatrix = viewProjection;
    m_UniformBuffer->SetData(&uniforms, sizeof(uniforms));

    // Both pipelines share the same bind-group layout at group 0, so one
    // SetBindGroup call persists across the SetPipeline switch.
    wgpuRenderPassEncoderSetBindGroup(passEncoder, 0, m_BindGroup, 0, nullptr);

    // Fills first — they write depth so the subsequent line pass (LessEqual,
    // depthWrite=false) can overlay cleanly at the same y.
    if (m_FillIndexCount > 0 && m_FillVertexBuffer && m_FillIndexBuffer)
    {
      wgpuRenderPassEncoderSetPipeline(passEncoder, m_FillPipeline);
      wgpuRenderPassEncoderSetVertexBuffer(passEncoder, 0, m_FillVertexBuffer->Buffer,
                                           0, m_FillVertexBuffer->Size);
      wgpuRenderPassEncoderSetIndexBuffer(passEncoder, m_FillIndexBuffer->Buffer,
                                          WGPUIndexFormat_Uint32, 0, m_FillIndexBuffer->Size);
      wgpuRenderPassEncoderDrawIndexed(passEncoder, m_FillIndexCount, 1, 0, 0, 0);
    }

    if (m_LineVertexCount > 0 && m_LineVertexBuffer)
    {
      const size_t lineSize = m_LineVertexCount * sizeof(TileVertex);
      wgpuRenderPassEncoderSetPipeline(passEncoder, m_LinePipeline);
      wgpuRenderPassEncoderSetVertexBuffer(passEncoder, 0, m_LineVertexBuffer->Buffer, 0, lineSize);
      wgpuRenderPassEncoderDraw(passEncoder, m_LineVertexCount, 1, 0, 0);
    }
  }
}  // namespace WebEngine
