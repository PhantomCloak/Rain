#include "ShaderManager.h"
#include <iostream>

// #include "src/tint/api/tint.h"
#include "core/Log.h"
#include "src/tint/lang/core/type/reference.h"
#include "src/tint/lang/core/type/storage_texture.h"
#include "src/tint/lang/wgsl/inspector/inspector.h"
#include "src/tint/lang/wgsl/reader/reader.h"

#include <tint.h>

#include "render/RenderContext.h"
#include "src/tint/lang/core/type/depth_texture.h"
#include "src/tint/lang/wgsl/ast/identifier.h"
#include "src/tint/lang/wgsl/ast/module.h"
#include "src/tint/lang/wgsl/sem/function.h"

std::map<std::string, Ref<Shader>> ShaderManager::m_Shaders;

WGPUTextureSampleType GetSampleType(tint::inspector::ResourceBinding::SampledKind kind) {
  switch (kind) {
    case tint::inspector::ResourceBinding::SampledKind::kFloat:
      return WGPUTextureSampleType_Float;
    case tint::inspector::ResourceBinding::SampledKind::kUInt:
      return WGPUTextureSampleType_Uint;
    case tint::inspector::ResourceBinding::SampledKind::kSInt:
      return WGPUTextureSampleType_Sint;
    case tint::inspector::ResourceBinding::SampledKind::kUnknown:
      return WGPUTextureSampleType_Undefined;
  }
}

WGPUTextureViewDimension GetDimensionType(tint::inspector::ResourceBinding::TextureDimension type) {
  switch (type) {
    case tint::inspector::ResourceBinding::TextureDimension::k1d:
      return WGPUTextureViewDimension_1D;
    case tint::inspector::ResourceBinding::TextureDimension::k2d:
      return WGPUTextureViewDimension_2D;
    case tint::inspector::ResourceBinding::TextureDimension::k2dArray:
      return WGPUTextureViewDimension_2DArray;
    case tint::inspector::ResourceBinding::TextureDimension::k3d:
      return WGPUTextureViewDimension_3D;
    case tint::inspector::ResourceBinding::TextureDimension::kCube:
      return WGPUTextureViewDimension_Cube;
    case tint::inspector::ResourceBinding::TextureDimension::kCubeArray:
      return WGPUTextureViewDimension_CubeArray;
    case tint::inspector::ResourceBinding::TextureDimension::kNone:
      return WGPUTextureViewDimension_Undefined;
      break;
  }
}

 WGPUTextureFormat GetImageFormat(tint::inspector::ResourceBinding::TexelFormat type) {
  switch (type) {
    case tint::inspector::ResourceBinding::TexelFormat::kRgba8Unorm:
      return WGPUTextureFormat_RGBA8Unorm;
    case tint::inspector::ResourceBinding::TexelFormat::kRgba32Float:
      return WGPUTextureFormat_RGBA32Float;
      break;
			default:
			 exit(1);
  }
}

ShaderReflectionInfo ReflectShader(Ref<Shader> shader) {
  ShaderReflectionInfo reflectionInfo;
  tint::wgsl::reader::Options options;
  options.allowed_features = {};

  tint::Source::File* file = new tint::Source::File(shader->GetName(), shader->GetSource());
  tint::Program program = tint::wgsl::reader::Parse(file, options);

  if (program.Diagnostics().contains_errors()) {
    RN_LOG_ERR("Shader Compilation Error: {0}", program.Diagnostics().str());
    return reflectionInfo;
  }

  tint::inspector::Inspector inspector(program);

  std::unordered_map<uint32_t, std::unordered_map<uint32_t, tint::inspector::ResourceBinding>> inspectorResourceBindings;

  auto& userTypes = reflectionInfo.UserTypes;
  auto& resourceBindings = reflectionInfo.ResourceDeclarations;

  // TODO: Check shader is valid
  for (auto& entryPoint : inspector.GetEntryPoints()) {
    auto textureBindings = inspector.GetSampledTextureResourceBindings(entryPoint.name);
    auto depthTextureBindings = inspector.GetDepthTextureResourceBindings(entryPoint.name);
    auto storageTextureBindings = inspector.GetStorageTextureResourceBindings(entryPoint.name);

    for (auto& entry : textureBindings) {
      inspectorResourceBindings[entry.bind_group][entry.binding] = entry;
    }
    for (auto& entry : depthTextureBindings) {
      inspectorResourceBindings[entry.bind_group][entry.binding] = entry;
    }
    for (auto& entry : storageTextureBindings) {
      inspectorResourceBindings[entry.bind_group][entry.binding] = entry;
    }
  }

  // TODO: If variable shared across two stage it will duplicate
  for (auto& entryPoint : inspector.GetEntryPoints()) {
    auto* func = program.AST().Functions().Find(program.Symbols().Get(entryPoint.name));
    auto* func_sem = program.Sem().Get(func);

    for (auto& ruv : func_sem->TransitivelyReferencedUniformVariables()) {
      auto* var = ruv.first;
      auto binding_info = ruv.second;
      auto* unwrapped_type = var->Type()->UnwrapRef();

      auto* uniformType = var->Type()->UnwrapRef()->As<tint::core::type::Struct>();
      auto typeName = uniformType->Name().Name();

      if (userTypes.find(typeName) == userTypes.end()) {
        for (const auto& member : uniformType->Members()) {
          auto memberName = member->Name().Name();
          userTypes[typeName][memberName] = {
              .Name = memberName,
              .Type = ShaderUniformType::Int,
              .Size = member->Size(),
              .Offset = member->Offset()};
        }
      }

      ResourceDeclaration info;
      info.Type = BindingType::UniformBindingType;
      info.Name = var->Declaration()->name->symbol.Name();
      info.GroupIndex = binding_info.group;
      info.LocationIndex = binding_info.binding;
      info.Size = unwrapped_type->Size();

      if (resourceBindings[info.GroupIndex].size() == 0) {
        resourceBindings[info.GroupIndex].push_back(info);
      }
    }

    for (auto& ruv : func_sem->TransitivelyReferencedSampledTextureVariables()) {
      auto* var = ruv.first;
      auto binding_info = ruv.second;
      auto* unwrapped_type = var->Type()->UnwrapRef();

      auto dec = var->Declaration();
      ResourceDeclaration info;
      info.Type = BindingType::TextureBindingType;
      info.Name = var->Declaration()->name->symbol.Name();
      info.GroupIndex = binding_info.group;
      info.LocationIndex = binding_info.binding;
      info.Size = unwrapped_type->Size();

      resourceBindings[info.GroupIndex].push_back(info);
    }

    for (auto* global : func_sem->TransitivelyReferencedGlobals()) {
      auto* unwrapped_type = global->Type()->UnwrapRef();
      auto* storage_texture = unwrapped_type->As<tint::core::type::StorageTexture>();

      // Only continue if the variable is a storage texture
      if (storage_texture == nullptr) {
        continue;
      }

      // Check if there is a binding point for this texture
      if (auto bp = global->Attributes().binding_point) {
        auto* unwrapped_type = global->Type()->UnwrapRef();
        auto binding_info = *bp;
        auto dec = global->Declaration();
        ResourceDeclaration info;
        info.Type = BindingType::StorageBindingType;
        info.Name = global->Declaration()->name->symbol.Name();
        info.GroupIndex = binding_info.group;
        info.LocationIndex = binding_info.binding;
        info.Size = unwrapped_type->Size();

        resourceBindings[info.GroupIndex].push_back(info);
      }
    }

    for (auto& ruv : func_sem->TransitivelyReferencedVariablesOfType(&tint::TypeInfo::Of<tint::core::type::DepthTexture>())) {
      auto* var = ruv.first;
      auto binding_info = ruv.second;
      auto* unwrapped_type = var->Type()->UnwrapRef();

      auto dec = var->Declaration();
      ResourceDeclaration info;
      info.Type = BindingType::TextureDepthBindingType;
      info.Name = var->Declaration()->name->symbol.Name();
      info.GroupIndex = binding_info.group;
      info.LocationIndex = binding_info.binding;
      info.Size = unwrapped_type->Size();

      resourceBindings[info.GroupIndex].push_back(info);
    }

    for (auto& ruv : func_sem->TransitivelyReferencedSamplerVariables()) {
      auto* var = ruv.first;
      auto binding_info = ruv.second;
      auto* unwrapped_type = var->Type()->UnwrapRef();

      ResourceDeclaration info;
      info.Type = BindingType::SamplerBindingType;
      info.Name = var->Declaration()->name->symbol.Name();
      info.GroupIndex = binding_info.group;
      info.LocationIndex = binding_info.binding;
      info.Size = unwrapped_type->Size();

      resourceBindings[info.GroupIndex].push_back(info);
    }

    for (auto& ruv : func_sem->TransitivelyReferencedComparisonSamplerVariables()) {
      auto* var = ruv.first;
      auto binding_info = ruv.second;
      auto* unwrapped_type = var->Type()->UnwrapRef();

      ResourceDeclaration info;
      info.Type = BindingType::CompareSamplerBindingType;
      info.Name = var->Declaration()->name->symbol.Name();
      info.GroupIndex = binding_info.group;
      info.LocationIndex = binding_info.binding;
      info.Size = unwrapped_type->Size();

      resourceBindings[info.GroupIndex].push_back(info);
    }
  }

  for (auto& [groupIndex, entries] : resourceBindings) {
    std::vector<WGPUBindGroupLayoutEntry> layoutEntries;
    std::sort(entries.begin(), entries.end(), [&](const ResourceDeclaration& current, const ResourceDeclaration& second) {
      return current.LocationIndex < second.LocationIndex;
    });

    for (const auto& entry : entries) {
      WGPUBindGroupLayoutEntry groupEntry = {};
      groupEntry.binding = entry.LocationIndex;
      groupEntry.nextInChain = nullptr;

      switch (entry.Type) {
        case UniformBindingType:
          groupEntry.buffer.type = WGPUBufferBindingType_Uniform;

          groupEntry.buffer.hasDynamicOffset = !entry.Name.empty() && entry.Name[0] == 'u' && entry.Name[1] == 'd';
          groupEntry.buffer.nextInChain = nullptr;
          groupEntry.buffer.minBindingSize = 0;
          groupEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex | WGPUShaderStage_Compute;
          break;
        case TextureBindingType:
          groupEntry.texture.sampleType = GetSampleType(inspectorResourceBindings[entry.GroupIndex][entry.LocationIndex].sampled_kind);
          groupEntry.texture.viewDimension = GetDimensionType(inspectorResourceBindings[entry.GroupIndex][entry.LocationIndex].dim);
          groupEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Compute;
          break;
        case TextureDepthBindingType:
          groupEntry.texture.sampleType = WGPUTextureSampleType_Depth;
          groupEntry.texture.viewDimension = GetDimensionType(inspectorResourceBindings[entry.GroupIndex][entry.LocationIndex].dim);
          groupEntry.visibility = WGPUShaderStage_Fragment;
          break;
        case SamplerBindingType:
          groupEntry.sampler.type = WGPUSamplerBindingType_Filtering;
          groupEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Compute;
          break;
        case CompareSamplerBindingType:
          groupEntry.sampler.type = WGPUSamplerBindingType_Comparison;
          groupEntry.visibility = WGPUShaderStage_Fragment;
          break;
        case StorageBindingType:
					auto v = inspectorResourceBindings[entry.GroupIndex][entry.LocationIndex].image_format;
          groupEntry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
          groupEntry.storageTexture.format = GetImageFormat(inspectorResourceBindings[entry.GroupIndex][entry.LocationIndex].image_format);
          groupEntry.storageTexture.viewDimension = GetDimensionType(inspectorResourceBindings[entry.GroupIndex][entry.LocationIndex].dim);

          //groupEntry.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;
          // groupEntry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;

          groupEntry.visibility = WGPUShaderStage_Compute;
          break;
      }

      layoutEntries.push_back(groupEntry);
    }

    std::string label = shader->GetName() + std::to_string(groupIndex);

    WGPUBindGroupLayoutDescriptor layoutDesc = {};
    layoutDesc.nextInChain = nullptr;
    layoutDesc.label = label.c_str();
    layoutDesc.entries = layoutEntries.data();
    layoutDesc.entryCount = layoutEntries.size();

    WGPUBindGroupLayout layout = wgpuDeviceCreateBindGroupLayout(RenderContext::GetDevice(), &layoutDesc);
    reflectionInfo.LayoutDescriptors[groupIndex] = layout;
  }

  return reflectionInfo;
}

Ref<Shader> ShaderManager::LoadShader(const std::string& shaderId,
                                      const std::string& shaderPath) {
  Ref<Shader> shader = Shader::Create(shaderId, shaderPath);
  auto reflectionInfo = ReflectShader(shader);

  shader->SetReflectionInfo(reflectionInfo);
  m_Shaders[shaderId] = shader;

  return shader;
};

Ref<Shader> ShaderManager::GetShader(const std::string& shaderId) {
  if (m_Shaders.find(shaderId) == m_Shaders.end()) {
    std::cout << "cannot find the shader";
    return nullptr;
  }

  return m_Shaders[shaderId];
}
