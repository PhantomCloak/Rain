#pragma once

#include <webgpu/webgpu.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "core/Ref.h"
#include "render/GPUAllocator.h"
#include "render/Shader.h"
#include "render/Framebuffer.h"

namespace WebEngine
{
  struct MVTTile;
  class MBTilesReader;

  struct TileVertex
  {
    glm::vec3 position;
    glm::vec4 color;
  };

  class VectorTileRenderer
  {
   public:
    void Init(Ref<Framebuffer> targetFramebuffer);
    // Load every tile inside [minTX..maxTX] x [minTY..maxTY] (inclusive). Tile
    // world positions are computed relative to (refTX, refTY) so the tile whose
    // coords match (ref, ref) sits at world origin — the caller picks the
    // reference to line up with its camera.
    void LoadTileRect(const MBTilesReader& source, int zoom,
                      int minTX, int minTY, int maxTX, int maxTY,
                      int refTX, int refTY);
    void Render(WGPURenderPassEncoder passEncoder, const glm::mat4& viewProjection);

   private:
    void AppendTileGeometry(const MVTTile& tile, glm::vec2 worldOffset, std::vector<TileVertex>& vertices);
    void UploadGeometry(const std::vector<TileVertex>& vertices);
    void CreatePipeline(Ref<Framebuffer> targetFramebuffer);

    static glm::vec4 GetLayerColor(const std::string& layerName);

    Ref<Shader> m_Shader;
    WGPURenderPipeline m_Pipeline = nullptr;
    WGPUBindGroup m_BindGroup = nullptr;
    Ref<GPUBuffer> m_VertexBuffer;
    Ref<GPUBuffer> m_UniformBuffer;
    uint32_t m_VertexCount = 0;
    bool m_Ready = false;
  };
}
