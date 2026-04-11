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

  // Renders vector-tile geometry as line segments. Tiles are fetched from an
  // MBTiles archive, parsed as MVT, converted to line-list vertices, and
  // uploaded into a single vertex buffer. Render() just issues the draw.
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
    void CreatePipeline(const Ref<Framebuffer>& targetFramebuffer);
    void UploadVertices(const std::vector<TileVertex>& vertices);

    Ref<Shader> m_Shader;
    WGPURenderPipeline m_Pipeline = nullptr;
    WGPUBindGroup m_BindGroup = nullptr;
    Ref<GPUBuffer> m_VertexBuffer;
    Ref<GPUBuffer> m_UniformBuffer;
    uint32_t m_VertexCount = 0;
    bool m_Ready = false;
  };
}  // namespace WebEngine
