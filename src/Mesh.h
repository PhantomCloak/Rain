#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "Node.h"
#include "render/Texture.h"

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

struct RenderMeshUniform {
  glm::mat4x4 modelMatrix;
  glm::vec4 color;
};

class Mesh : public Node {
 public:
  // mesh data
  RenderMeshUniform uniform;
  std::vector<VertexE> vertices;
  std::vector<unsigned int> indices;

  std::shared_ptr<Texture> textureDiffuse;

  Mesh(std::vector<VertexE> vertices,
        std::vector<unsigned int> indices,
        std::shared_ptr<Texture> textureDiffuse,
        WGPUBindGroupLayout resourceLayout,
				WGPUDevice& device,
				WGPUQueue& queue,
				WGPUSampler& sampler);

  void Draw(WGPURenderPassEncoder& renderPass, WGPURenderPipeline& pipeline);
	void UpdateUniforms(WGPUQueue& queue);

 private:
  WGPUBindGroup defaultResourcesBindGroup;
  WGPUBuffer uniformBuffer;
  WGPUBuffer vertexBuffer;
  WGPUBuffer indexBuffer;
};
