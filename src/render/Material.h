#pragma once
#include <glm/glm.hpp>
#include "core/UUID.h"
#include "render/BindingManager.h"
#include "render/GPUAllocator.h"
#include "render/Texture.h"

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

class Material {
 public:
  UUID Id = 0;
  std::string name;
  MaterialUniform properties;

  std::vector<Ref<Texture>> m_diffuseTextures;

  //WGPUBindGroup bgMaterial;
	BindingManager* bindingManager;
  Ref<GPUBuffer> materialUniformBuffer;

  void SetDiffuseTexture(const std::string& name, const Ref<Texture> value);

  //static void CreateMaterial(Ref<Material> mat);
  static void CreateMaterial(Ref<Material> mat, Ref<Shader> shader);
};
