#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "Node.h"
#include "render/Texture.h"

struct VertexAttribute {
  glm::vec3 Position;
	float _pad0;
  glm::vec3 Normal;
	float _pad1;
  glm::vec2 TexCoords;
	float _pad2[2];
	glm::vec3 Tangent;
	float _pad3;
	glm::vec3 BitTangent;
	float _pad4;
};

struct RenderMeshUniform {
  glm::mat4x4 modelMatrix;
  glm::vec4 color;
};

class Mesh : public Node {
 public:
  // mesh data
  RenderMeshUniform uniform;
  std::vector<VertexAttribute> vertices;
  std::vector<unsigned int> indices;

  std::shared_ptr<Texture> textureDiffuse;

  Mesh(std::vector<VertexAttribute> vertices,
        std::vector<unsigned int> indices,
        std::shared_ptr<Texture> textureDiffuse,
        std::shared_ptr<Texture> textureHeight,
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
