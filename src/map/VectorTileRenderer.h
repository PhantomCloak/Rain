#pragma once

#include <webgpu/webgpu.h>
#include <glm/glm.hpp>
#include <vector>
#include "core/Ref.h"
#include "render/Framebuffer.h"
#include "render/GPUAllocator.h"
#include "render/Shader.h"

namespace WebEngine
{
  struct MVTTile;
  class MBTilesReader;

  struct TileVertex
  {
    glm::vec3 position;
    glm::vec4 color;
  };

  // Renders vector-tile geometry. MVT polygon features are triangulated into
  // filled regions via shoelace-based ring classification + earcut, while
  // linestring features and tile borders are emitted as line segments. Two
  // pipelines share a single shader / uniform buffer / bind group and differ
  // only in primitive topology and depth state.
  class VectorTileRenderer
  {
   public:
    void Init(const Ref<Framebuffer>& targetFramebuffer);

    // Load every tile inside [minTX..maxTX] x [minTY..maxTY] (inclusive). Tile
    // world positions are computed relative to (refTX, refTY) so the tile at
    // (ref, ref) sits at world origin — the caller picks the reference to line
    // up with its camera.
    void LoadTileRect(const MBTilesReader& source, int zoom,
                      int minTX, int minTY, int maxTX, int maxTY,
                      int refTX, int refTY);

    void Render(WGPURenderPassEncoder passEncoder, const glm::mat4& viewProjection);

   private:
    void CreatePipelines(const Ref<Framebuffer>& targetFramebuffer);
    void UploadGeometry(const std::vector<TileVertex>& lineVerts,
                        const std::vector<TileVertex>& fillVerts,
                        const std::vector<uint32_t>& fillIndices);

    Ref<Shader> m_Shader;
    WGPURenderPipeline m_LinePipeline = nullptr;
    WGPURenderPipeline m_FillPipeline = nullptr;
    WGPUBindGroup m_BindGroup = nullptr;
    Ref<GPUBuffer> m_LineVertexBuffer;
    Ref<GPUBuffer> m_FillVertexBuffer;
    Ref<GPUBuffer> m_FillIndexBuffer;
    Ref<GPUBuffer> m_UniformBuffer;
    uint32_t m_LineVertexCount = 0;
    uint32_t m_FillIndexCount = 0;
    bool m_Ready = false;
  };
}  // namespace WebEngine
