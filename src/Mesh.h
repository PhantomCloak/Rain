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

struct SceneUniform {
  glm::mat4x4 modelMatrix;
  glm::vec4 color;
};

struct MaterialUniform {
  glm::vec3 ambientColor;
	float _pad0 = 0.0;
  glm::vec3 diffuseColor;
	float _pad1 = 0.0;
  glm::vec3 specularColor;
	float shininess;
};

class Mesh : public Node {
 public:
  // mesh data
  SceneUniform sceneUniform;
  MaterialUniform materialUniform;
  std::vector<VertexAttribute> vertices;
  std::vector<unsigned int> indices;

  std::shared_ptr<Texture> textureDiffuse;

  Mesh(std::vector<VertexAttribute> vertices,
        std::vector<unsigned int> indices,
        std::shared_ptr<Texture> textureDiffuse,
        std::shared_ptr<Texture> textureHeight,
				MaterialUniform material,
        WGPUBindGroupLayout resourceLayout,
				WGPUDevice& device,
				WGPUQueue& queue,
				WGPUSampler& sampler);

  void Draw(WGPURenderPassEncoder& renderPass, WGPURenderPipeline& pipeline);
	void UpdateUniforms(WGPUQueue& queue);

 private:
  WGPUBindGroup defaultResourcesBindGroup;
  WGPUBuffer sceneUniformBuffer;
  WGPUBuffer materialUniformBuffer;
  WGPUBuffer vertexBuffer;
  WGPUBuffer indexBuffer;
};
