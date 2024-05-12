#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "Node.h"
#include "render/primitives/RenderMeshUniform.h"
#include "render/primitives/texture.h"

struct VertexE {
  glm::vec3 Position;
  glm::vec3 Normal;
  glm::vec2 TexCoords;
};

struct VertexAttributes {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
};

class MeshE : public Node {
 public:
  // mesh data
  RenderMeshUniform uniform;
  std::vector<VertexE> vertices;
  std::vector<unsigned int> indices;

  std::shared_ptr<Texture> textureDiffuse;

  MeshE(std::vector<VertexE> vertices,
        std::vector<unsigned int> indices,
        std::shared_ptr<Texture> textureDiffuse,
        WGPUBindGroupLayout resourceLayout,
				WGPUDevice& device,
				WGPUQueue& queue,
				WGPUSampler& sampler);

  void Draw(WGPURenderPassEncoder& renderPass, WGPURenderPipeline& pipeline);

 private:
  WGPUBindGroup defaultResourcesBindGroup;
  WGPUBuffer uniformBuffer;
  WGPUBuffer vertexBuffer;
  WGPUBuffer indexBuffer;
};
