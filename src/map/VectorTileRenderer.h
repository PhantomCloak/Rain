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

  struct TileVertex
  {
    glm::vec3 position;
    glm::vec4 color;
  };

  class VectorTileRenderer
  {
   public:
    void Init(Ref<Framebuffer> targetFramebuffer);
    void LoadTile(const std::string& path);
    void Render(WGPURenderPassEncoder passEncoder, const glm::mat4& viewProjection);

   private:
    void BuildGeometry(const MVTTile& tile);
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
