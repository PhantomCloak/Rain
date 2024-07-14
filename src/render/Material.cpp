#include "Material.h"
#include "render/Render.h"

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

Material::Material(const std::string& name, Ref<Shader> shader)
    : m_Name(name) {
  const BindingSpec spec = {
      .Name = "BG_" + name,
      .Shader = shader,
      .DefaultResources = true};

  m_BindManager = BindingManager::Create(spec);

  for (const auto& [name, decl] : m_BindManager->InputDeclarations) {
    if (decl.Group != 1) {
      continue;
    }
    if (decl.Type == RenderPassResourceType::PT_Texture) {
      m_BindManager->Set(decl.Name, Render::GetWhiteTexture());
    }
    if (decl.Type == RenderPassResourceType::PT_Sampler) {
      m_BindManager->Set(decl.Name, Render::GetDefaultSampler());
    }
  }

  m_UBMaterial = GPUAllocator::GAlloc(name, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, sizeof(MaterialUniform));
}

Ref<Material> Material::CreateMaterial(const std::string& name, Ref<Shader> shader) {
  return CreateRef<Material>(name, shader);
};

void Material::Bake() {
  m_BindManager->Bake();
}

const WGPUBindGroup& Material::GetBinding(int index) {
  return m_BindManager->GetBindGroup(index);
}

void Material::SetDiffuseTexture(const std::string& name, const Ref<Texture> value) {
  m_diffuseTextures.push_back(value);
}

void Material::Set(const std::string& name, Ref<Texture> texture) {
  m_BindManager->Set(name, texture);
}
void Material::Set(const std::string& name, Ref<GPUBuffer> uniform) {
  m_BindManager->Set(name, uniform);
}
void Material::Set(const std::string& name, Ref<Sampler> sampler) {
  m_BindManager->Set(name, sampler);
}

void Material::Set(const std::string& name, const MaterialProperties& props) {
  MaterialUniform un;
  un.ambientColor = props.ambientColor,
  un.diffuseColor = props.diffuseColor,
  un.specularColor = props.specularColor,
  un.shininess = props.shininess;

  m_UBMaterial->SetData(&un, sizeof(MaterialUniform));
  m_BindManager->Set(name, m_UBMaterial);
}
