#include "Material.h"
#include <iostream>
#include <ostream>
#include "render/GPUAllocator.h"
#include "render/ResourceManager.h"

void Material::CreateMaterial(Ref<Material> mat, Ref<Shader> shader) {
  static Ref<Texture> defaultTexture = Rain::ResourceManager::GetTexture("T_Default");
  RN_ASSERT(defaultTexture->View != 0, "Material: Default texture couldn't found.");

  if (mat->m_diffuseTextures.size() <= 0 || mat->m_diffuseTextures[0]->View == 0) {
    std::cout << "Diffuse texture is not exist." << std::endl;
    mat->m_diffuseTextures.push_back(Rain::ResourceManager::GetTexture("T_Default"));
  }

  mat->materialUniformBuffer = GPUAllocator::GAlloc(mat->name, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, sizeof(MaterialUniform));
  mat->materialUniformBuffer->SetData(&mat->properties, sizeof(MaterialUniform));

	static Ref<Sampler> s_MaterialSampler = Sampler::Create(Sampler::GetDefaultProps());

	const BindingSpec spec = {
		.Shader = shader
	};

	mat->bindingManager = new BindingManager(spec);

	mat->bindingManager->Set("gradientTexture", mat->m_diffuseTextures[0]);
	mat->bindingManager->Set("textureSampler", s_MaterialSampler);
	mat->bindingManager->Set("uMaterial", mat->materialUniformBuffer);
	mat->bindingManager->Bake();
};

void Material::SetDiffuseTexture(const std::string& name, const Ref<Texture> value) {
  m_diffuseTextures.push_back(value);
}
