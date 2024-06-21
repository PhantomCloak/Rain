#include "BindingManager.h"

#include <tint/tint.h>
#include <iostream>

void BindingManager::Set(const std::string& name, Ref<Texture> texture) {
  m_Inputs.push_back({.Name = name, .TextureInput = texture});
}

void BindingManager::Set(const std::string& name, Ref<GPUBuffer> uniform) {
  m_Inputs.push_back({.Name = name, .UniformIntput = uniform});
}

void BindingManager::Set(const std::string& name, Ref<Sampler> sampler) {
  m_Inputs.push_back({.Name = name, .SamplerInput = sampler});
}

const ResourceDeclaration& BindingManager::GetInfo(const std::string& name) {
  auto& resourceBindings = m_BindingSpec.Shader->GetReflectionInfo().ResourceDeclarations;
  for (auto& [_, entries] : resourceBindings) {
    for (auto& entry : entries) {
      if (entry.Name == name) {
        return entry;
      }
    }
  }

  RN_ASSERT("Binding entry couldn't found");
  return {};
}

void BindingManager::Bake() {
  static int debugBake = 0;
  std::cout << "DEBUG: " << debugBake << std::endl;
  std::map<int, std::vector<WGPUBindGroupEntry>> shaderEntries;
  static auto s_DefaultSampler = Sampler::Create(Sampler::GetDefaultProps());

  for (auto& input : m_Inputs) {
    auto& bindingInfo = GetInfo(input.Name);

    WGPUBindGroupEntry entry = {};
    entry.binding = bindingInfo.LocationIndex;
    entry.offset = 0;
    entry.size = bindingInfo.Size;
    entry.nextInChain = nullptr;

    switch (bindingInfo.Type) {
      case UniformBindingType:
        entry.buffer = input.UniformIntput->Buffer;
        break;
      case TextureBindingType:
        entry.textureView = input.TextureInput->View;
        break;
      case SamplerBindingType:
        entry.sampler = *input.SamplerInput->GetNativeSampler();
        break;
      case CompareSamplerBindingType:
        entry.sampler = *input.SamplerInput->GetNativeSampler();
        break;
    }

    shaderEntries[bindingInfo.GroupIndex].push_back(entry);
  }

  for (auto [groupIndex, entries] : shaderEntries) {
    std::sort(entries.begin(), entries.end(), [](WGPUBindGroupEntry& first, WGPUBindGroupEntry& other) {
      return first.binding < other.binding;
    });

    std::string boo = "bg_mesh_mat" + std::to_string(debugBake);
    WGPUBindGroupDescriptor bgDesc = {.label = boo.c_str()};
    bgDesc.layout = m_BindingSpec.Shader->GetReflectionInfo().LayoutDescriptors[groupIndex];
    bgDesc.entryCount = (uint32_t)entries.size();
    bgDesc.entries = entries.data();

    m_BindGroups[groupIndex] = wgpuDeviceCreateBindGroup(RenderContext::GetDevice(), &bgDesc);
  }
  debugBake++;
}
