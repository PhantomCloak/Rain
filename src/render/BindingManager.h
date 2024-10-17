#pragma once

#include <map>
#include <string>
#include "render/GPUAllocator.h"
#include "render/Sampler.h"
#include "render/Shader.h"
#include "render/Texture.h"

enum RenderPassResourceType  {
	PT_Uniform,
	PT_Texture,
	PT_Sampler
};

struct BindingSpec {
  std::string Name;
  Ref<Shader> ShaderRef;
  bool DefaultResources;
};

struct RenderPassInput {
	RenderPassResourceType Type;
  Ref<Texture> TextureInput = nullptr;
  Ref<GPUBuffer> UniformIntput = nullptr;
  Ref<Sampler> SamplerInput = nullptr;
};


struct RenderPassInputDeclaration { 
	std::string Name;
	RenderPassResourceType Type;
	uint32_t Group;
	uint32_t Location;
};

class BindingManager {
 public:
  BindingManager() {};
  BindingManager(const BindingSpec& spec);
  static Ref<BindingManager> Create(const BindingSpec& spec) { return CreateRef<BindingManager>(spec); }

  std::map<int, WGPUBindGroup>& GetBindGroups() { return m_BindGroups; }
  const WGPUBindGroup& GetBindGroup(int group) { return m_BindGroups[group]; }
  void Set(const std::string& name, Ref<Texture> texture);
  void Set(const std::string& name, Ref<GPUBuffer> uniform);
  void Set(const std::string& name, Ref<Sampler> sampler);
	void Init();
	bool Validate();
  void Bake();
	void InvalidateAndUpdate();

	const RenderPassInputDeclaration* GetInputDeclaration(const std::string& name) const;

	std::map<std::string, RenderPassInputDeclaration> InputDeclarations;
 private:
  const ResourceDeclaration& GetInfo(const std::string& name) const;

  std::map<int, WGPUBindGroup> m_BindGroups;

  std::map<uint32_t, std::map<uint32_t, WGPUBindGroupEntry>> m_GroupEntryMap;

  std::map<uint32_t, std::map<uint32_t, RenderPassInput>> m_Inputs;
  std::map<uint32_t, std::map<uint32_t, RenderPassInput>> m_InvalidatedInputs;

  BindingSpec m_BindingSpec;
};
