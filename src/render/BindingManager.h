#pragma once

#include <map>
#include <string>
#include "render/GPUAllocator.h"
#include "render/Sampler.h"
#include "render/Shader.h"
#include "render/Texture.h"

struct BindingSpec {
  Ref<Shader> Shader;
};

struct Input {
  std::string Name;
  Ref<Texture> TextureInput = nullptr;
  Ref<GPUBuffer> UniformIntput = nullptr;
  Ref<Sampler> SamplerInput = nullptr;
};

class BindingManager {
 public:
  BindingManager() {};
  BindingManager(const BindingSpec& spec)
      : m_BindingSpec(spec) {};

  WGPUBindGroup GetBindGroup(int group) { return m_BindGroups[group]; }
  void Set(const std::string& name, Ref<Texture> texture);
  void Set(const std::string& name, Ref<GPUBuffer> uniform);
  void Set(const std::string& name, Ref<Sampler> sampler);
  void Bake();

 private:
  const ResourceDeclaration& GetInfo(const std::string& name);

  std::map<int, WGPUBindGroup> m_BindGroups;
  std::vector<Input> m_Inputs;
  BindingSpec m_BindingSpec;
};
