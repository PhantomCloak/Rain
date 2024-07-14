#pragma once
#include <glm/glm.hpp>
#include "core/UUID.h"
#include "render/BindingManager.h"
#include "render/GPUAllocator.h"
#include "render/Texture.h"

struct MaterialProperties {
  glm::vec3 ambientColor;
  glm::vec3 diffuseColor;
  glm::vec3 specularColor;
  float shininess;
};

class Material {
 public:
  UUID Id;

  std::vector<Ref<Texture>> m_diffuseTextures;
  void SetDiffuseTexture(const std::string& name, const Ref<Texture> value);

  void Set(const std::string& name, Ref<Texture> texture);
  void Set(const std::string& name, Ref<GPUBuffer> uniform);
  void Set(const std::string& name, Ref<Sampler> sampler);
  void Set(const std::string& name, const MaterialProperties& props);

  Material(const std::string& name, Ref<Shader> shader);

  void Bake();
  const std::string& GetName() { return m_Name; }

  const WGPUBindGroup& GetBinding(int index);
  static Ref<Material> CreateMaterial(const std::string& name, Ref<Shader> shader);

  Ref<BindingManager> m_BindManager;

 private:
  std::string m_Name;
  Ref<GPUBuffer> m_UBMaterial;
};

class MaterialTable {
 public:
  MaterialTable(uint32_t materialCount = 1) {};
  ~MaterialTable() = default;

  bool HasMaterial(uint32_t materialIndex) const { return m_Materials.find(materialIndex) != m_Materials.end(); }
  void SetMaterial(uint32_t index, Ref<Material> material) { m_Materials[index] = material; }
  void ClearMaterial(uint32_t index);

  Ref<Material> GetMaterial(uint32_t materialIndex) const {
    return m_Materials.at(materialIndex);
  }
  std::map<uint32_t, Ref<Material>>& GetMaterials() { return m_Materials; }
  const std::map<uint32_t, Ref<Material>>& GetMaterials() const { return m_Materials; }

  uint32_t GetMaterialCount() const { return m_MaterialCount; }
  void SetMaterialCount(uint32_t materialCount) { m_MaterialCount = materialCount; }

  void Clear();

 private:
  std::map<uint32_t, Ref<Material>> m_Materials;
  uint32_t m_MaterialCount;
};
