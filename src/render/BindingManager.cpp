#include "BindingManager.h"
#include "render/Render.h"

RenderPassResourceType GetRenderPassTypeFromShaderDecl(BindingType type) {
  switch (type) {
    case UniformBindingType:
      return RenderPassResourceType::PT_Uniform;
    case TextureBindingType:
      return RenderPassResourceType::PT_Texture;
    case TextureDepthBindingType:
      return RenderPassResourceType::PT_Texture;
    case SamplerBindingType:
      return RenderPassResourceType::PT_Sampler;
    case CompareSamplerBindingType:
      return RenderPassResourceType::PT_Sampler;
      break;
  }

  RN_ASSERT("Unkown type conversation");
}

bool IsInputEmpty(const RenderPassInput& input) {
  return input.SamplerInput == nullptr && input.UniformIntput == nullptr && input.TextureInput == nullptr;
}

BindingManager::BindingManager(const BindingSpec& spec)
    : m_BindingSpec(spec) {
  Init();
}

void BindingManager::Set(const std::string& name, Ref<Texture> texture) {
  const auto* decl = GetInputDeclaration(name);
  if (decl != nullptr) {
    m_Inputs[decl->Group][decl->Location].Type = RenderPassResourceType::PT_Texture;
    m_Inputs[decl->Group][decl->Location].TextureInput = texture;
  } else {
    RN_LOG_ERR("Input Texture {} does not exist on {} Shader.", name, m_BindingSpec.Shader->GetName());
  }
}

void BindingManager::Set(const std::string& name, Ref<GPUBuffer> uniform) {
  const auto* decl = GetInputDeclaration(name);

  if (decl != nullptr) {
    m_Inputs[decl->Group][decl->Location].Type = RenderPassResourceType::PT_Uniform;
    m_Inputs[decl->Group][decl->Location].UniformIntput = uniform;
  } else {
    RN_LOG_ERR("Input Uniform {} does not exist on {} Shader.", name, m_BindingSpec.Shader->GetName());
  }
}

void BindingManager::Set(const std::string& name, Ref<Sampler> sampler) {
  const auto* decl = GetInputDeclaration(name);
  if (decl != nullptr) {
    m_Inputs[decl->Group][decl->Location].Type = RenderPassResourceType::PT_Sampler;
    m_Inputs[decl->Group][decl->Location].SamplerInput = sampler;
  } else {
    RN_LOG_ERR("Input Sampler {} does not exist on {} Shader.", name, m_BindingSpec.Shader->GetName());
  }
}

const ResourceDeclaration& BindingManager::GetInfo(const std::string& name) const {
  auto& resourceBindings = m_BindingSpec.Shader->GetReflectionInfo().ResourceDeclarations;
  for (auto& [_, entries] : resourceBindings) {
    for (auto& entry : entries) {
      if (entry.Name == name) {
        return entry;
      }
    }
  }

  RN_ASSERT(false, "Binding entry couldn't found");
}

// There is still some API limitation with deffered bindings

void BindingManager::Init() {
  auto shaderDecls = m_BindingSpec.Shader->GetReflectionInfo().ResourceDeclarations;

  for (const auto& [groupIndex, decls] : shaderDecls) {
    for (const auto& decl : decls) {
      RenderPassInputDeclaration& input = InputDeclarations[decl.Name];
      input.Name = decl.Name;
      input.Type = GetRenderPassTypeFromShaderDecl(decl.Type);
      input.Group = groupIndex;
      input.Location = decl.LocationIndex;
    }
  }
}

bool BindingManager::Validate() {
  auto shaderDecls = m_BindingSpec.Shader->GetReflectionInfo().ResourceDeclarations;

  for (const auto& [group, decls] : shaderDecls) {
    for (const auto& decl : decls) {
      if (InputDeclarations.find(decl.Name) == InputDeclarations.end()) {
        RN_LOG_ERR("Input won't exist on the shader.");
        return false;
      }

      if (m_Inputs.find(group) != m_Inputs.end() && m_Inputs.at(group).find(decl.LocationIndex) != m_Inputs.at(group).end()) {
        if (IsInputEmpty(m_Inputs.at(group).at(decl.LocationIndex))) {
          RN_LOG_ERR("Input {} is empty! ", decl.Name);
          return false;
        }
      }
    }
  }

  return true;
}

void BindingManager::Bake() {
  //if (!Validate()) {
  //  RN_LOG_ERR("Binding validation failed for {}.", m_BindingSpec.Name);
  //  return;
  //}

  for (const auto& [groupIndex, groupBindings] : m_Inputs) {
    std::vector<WGPUBindGroupEntry> shaderEntries;
    for (const auto& [location, input] : groupBindings) {
      WGPUBindGroupEntry entry = {};
      entry.binding = location;
      entry.offset = 0;
      entry.nextInChain = nullptr;

      switch (input.Type) {
        case PT_Uniform:
          entry.buffer = input.UniformIntput->Buffer;
          entry.size = input.UniformIntput->Size;
          break;
        case PT_Texture:
          entry.textureView = input.TextureInput->View;
          break;
        case PT_Sampler:
          entry.sampler = *input.SamplerInput->GetNativeSampler();
          break;
      }
      shaderEntries.push_back(entry);
    }

		if(shaderEntries.empty())
		{
			continue;
		}

    std::string label = m_BindingSpec.Name;
    WGPUBindGroupDescriptor bgDesc = {.label = label.c_str()};
    bgDesc.layout = m_BindingSpec.Shader->GetReflectionInfo().LayoutDescriptors[groupIndex];
    bgDesc.entryCount = (uint32_t)shaderEntries.size();
    bgDesc.entries = shaderEntries.data();

    auto wgpuBindGroup = wgpuDeviceCreateBindGroup(RenderContext::GetDevice(), &bgDesc);

    RN_ASSERT(wgpuBindGroup != 0, "BindGroup creation failed.");
    RN_LOG("BG {}", (void*)wgpuBindGroup);

    m_BindGroups[groupIndex] = wgpuBindGroup;
  }
}

const RenderPassInputDeclaration* BindingManager::GetInputDeclaration(const std::string& name) const {
  std::string nameStr(name);
  if (InputDeclarations.find(nameStr) == InputDeclarations.end()) {
    return nullptr;
  }

  const RenderPassInputDeclaration& decl = InputDeclarations.at(nameStr);
  return &decl;
}
