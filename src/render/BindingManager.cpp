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

bool IsInputValid(const RenderPassInput& input) {
  switch (input.Type) {
    case PT_Uniform:
      return input.UniformIntput != NULL && input.UniformIntput->Buffer != NULL;
    case PT_Texture:
      return input.TextureInput != NULL && input.TextureInput->View != NULL;
    case PT_Sampler:
      return input.SamplerInput != NULL && input.SamplerInput->GetNativeSampler() != NULL;
    default:
      return false;
  }
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
  auto& shaderName = m_BindingSpec.Shader->GetName();
  auto& shaderDecls = m_BindingSpec.Shader->GetReflectionInfo().ResourceDeclarations;

  for (const auto& [groupIndex, decls] : shaderDecls) {
    for (const auto& decl : decls) {
      auto inputIterator = InputDeclarations.find(decl.Name);

      if (inputIterator == InputDeclarations.end()) {
        RN_LOG_ERR("Validation failed: InputDeclaration '{}' not found in shader '{}'.", decl.Name, shaderName);
        return false;
      }

      auto& inputDecl = inputIterator->second;

      if (m_Inputs.find(inputDecl.Group) != m_Inputs.end()) {
        // RN_LOG_ERR("Validation failed: Input group '{}' not found in shader '{}'.", inputDecl.Group, shaderName);
        // return false;
        if (m_Inputs.at(inputDecl.Group).find(inputDecl.Location) == m_Inputs.at(inputDecl.Group).end()) {
          RN_LOG_ERR("Validation failed: Input '{}' from group '{}' does not exist at location '{}' in shader '{}'.", decl.Name, inputDecl.Group, inputDecl.Location, shaderName);
          return false;
        }

        if (!IsInputValid(m_Inputs.at(inputDecl.Group).at(inputDecl.Location))) {
          RN_LOG_ERR("Validation failed: Input '{}' is invalid in shader '{}'.", decl.Name, shaderName);
          return false;
        }
      }
    }
  }

  return true;
}

void BindingManager::Bake() {
  RN_CORE_ASSERT(Validate(), "Validation failed for {}", m_BindingSpec.Name)

  for (const auto& [groupIndex, groupBindings] : m_Inputs) {
    std::vector<WGPUBindGroupEntry> shaderEntries;
    for (const auto& [locationIndex, input] : groupBindings) {
      WGPUBindGroupEntry entry = {};
      entry.binding = locationIndex;
      entry.offset = 0;
      entry.nextInChain = nullptr;

      // Since we dont support postponed resources we simply assert
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

    if (shaderEntries.empty()) {
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
