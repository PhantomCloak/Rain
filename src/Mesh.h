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
};

struct SceneUniformBatch {
  glm::mat4x4 modelMatrixArr[16];
  glm::vec4 color;
};

struct SceneUniform {
  glm::mat4x4 modelMatrix;
  glm::vec4 color;
};

typedef unsigned long UUID;

struct MaterialUniform {
  glm::vec3 ambientColor;
	float _pad0 = 0.0;
  glm::vec3 diffuseColor;
	float _pad1 = 0.0;
  glm::vec3 specularColor;
	float shininess;

	MaterialUniform() {
		ambientColor = glm::vec3(0);
		diffuseColor = glm::vec3(0);
		specularColor = glm::vec3(0);
		shininess = -1;
	}
};

static UUID nextId = 0;

struct Material {
	UUID Id;
	MaterialUniform properties;

	Material() {
		Id = nextId++;
	}
};

class Mesh {
 public:
  // mesh data
	UUID Id;
  std::vector<VertexAttribute> vertices;
  std::vector<unsigned int> indices;

  std::shared_ptr<Texture> textureDiffuse;

  Mesh(std::vector<VertexAttribute> vertices,
        std::vector<unsigned int> indices);

  WGPUBuffer vertexBuffer;
  WGPUBuffer indexBuffer;
};
