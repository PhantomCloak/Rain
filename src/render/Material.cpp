#include "Material.h"
#include "render/Render.h"


Material::Material(const std::string& name, Ref<Shader> shader)
    : m_Name(name) {
  const BindingSpec spec = {
      .Name = "BG_" + name,
      .ShaderRef = shader,
      .DefaultResources = true};

  m_BindManager = BindingManager::Create(spec);
	m_Shader = shader;

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

	int size = 0;
	for(const auto& [_, member] : m_Shader->GetReflectionInfo().UserTypes[MATERIAL_UNIFORM_KEY])
	{
		size += member.Size;
	}
	m_UniformStorageBuffer.Allocate(size);
	m_UniformStorageBuffer.ZeroInitialize();

  m_UBMaterial = GPUAllocator::GAlloc(name, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, size);
	m_BindManager->Set("uMaterial", m_UBMaterial);
}

Ref<Material> Material::CreateMaterial(const std::string& name, Ref<Shader> shader) {
  return CreateRef<Material>(name, shader);
};

void Material::Bake() {
	m_UBMaterial->SetData(m_UniformStorageBuffer.Data, m_UniformStorageBuffer.GetSize());
  m_BindManager->Bake();
}

const WGPUBindGroup& Material::GetBinding(int index) {
	m_BindManager->InvalidateAndUpdate();
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

void Material::Set(const std::string& name, float value) {
	Set<float>(name, value);
}

void Material::Set(const std::string& name, int value) {
	Set<int>(name, value);
}

void Material::Set(const std::string& name, bool value) {
	Set<int>(name, (int)value);
}

const ShaderTypeDecl& Material::FindShaderUniformDecl(const std::string& name) {
	return m_Shader->GetReflectionInfo().UserTypes[MATERIAL_UNIFORM_KEY][name];
}
