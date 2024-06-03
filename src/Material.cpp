#include "Material.h"
#include <iostream>
#include <ostream>
#include "render/GPUAllocator.h"
#include "render/Render.h"
#include "render/RenderUtils.h"
#include "render/ResourceManager.h"

void MaterialEngine::CreateMaterial(Ref<MaterialEngine> mat) {
  static Ref<Texture> def = Rain::ResourceManager::GetTexture("T_Default");

  static GroupLayout materialGroup = {
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {1, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler},
      {2, GroupLayoutVisibility::Both, GroupLayoutType::Uniform}};
  static WGPUBindGroupLayout bglMaterial = LayoutUtils::CreateBindGroup("bgl_mesh_mat", Render::Instance->m_device, materialGroup);

	if(mat->m_diffuseTextures.size() <= 0 || mat->m_diffuseTextures[0]->View == 0)
	{
		std::cout << "Diffuse texture is not exist." << std::endl;
		mat->m_diffuseTextures.push_back(Rain::ResourceManager::GetTexture("T_Default"));
	}

  mat->materialUniformBuffer = GPUAllocator::GAlloc(mat->name, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, sizeof(MaterialUniform));


	std::cout << "Texture View: " << mat->m_diffuseTextures[0]->View << std::endl;
	std::cout << "Sampler: " << Render::Instance->m_sampler << std::endl;
  static std::vector<WGPUBindGroupEntry> bindingsMaterial(3);

  bindingsMaterial[0].binding = 0;
  bindingsMaterial[0].offset = 0;
	bindingsMaterial[0].textureView = mat->m_diffuseTextures[0]->View;

  bindingsMaterial[1].binding = 1;
  bindingsMaterial[1].offset = 0;
  bindingsMaterial[1].sampler = Render::Instance->m_sampler;  // Default sampler

  bindingsMaterial[2].binding = 2;
  bindingsMaterial[2].buffer = mat->materialUniformBuffer.Buffer;
  bindingsMaterial[2].offset = 0;
  bindingsMaterial[2].size = sizeof(MaterialUniform);

  WGPUBindGroupDescriptor bgMaterialDesc = {.label = "bg_mesh_mat"};
  bgMaterialDesc.layout = bglMaterial;
  bgMaterialDesc.entryCount = (uint32_t)bindingsMaterial.size();
  bgMaterialDesc.entries = bindingsMaterial.data();

  mat->bgMaterial = wgpuDeviceCreateBindGroup(Render::Instance->m_device, &bgMaterialDesc);

  wgpuQueueWriteBuffer(Render::Instance->m_queue, mat->materialUniformBuffer.Buffer, 0, &mat->properties, sizeof(MaterialUniform));
};

void MaterialEngine::SetDiffuseTexture(const std::string& name, const Ref<Texture> value) {
	m_diffuseTextures.push_back(value);
}
