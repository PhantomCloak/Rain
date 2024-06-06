#pragma once
#include <glm/glm.hpp>
#include "render/GPUAllocator.h"
#include "Mesh.h"

class MaterialEngine {
	public:
	UUID Id;
	std::string name;
	MaterialUniform properties;

	std::vector<Ref<Texture>> m_diffuseTextures;

	WGPUBindGroup bgMaterial;
  GPUBuffer materialUniformBuffer;

	void SetDiffuseTexture(const std::string& name, const Ref<Texture> value);

	static void CreateMaterial(Ref<MaterialEngine> mat);
};
