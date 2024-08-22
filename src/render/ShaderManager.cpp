#include "ShaderManager.h"
#include <iostream>

//#include "src/tint/api/tint.h"
#include "src/tint/lang/wgsl/inspector/inspector.h"
#include "src/tint/lang/wgsl/reader/reader.h"

#include <tint.h>

#include "render/RenderContext.h"
#include "src/tint/lang/core/type/depth_texture.h"
#include "src/tint/lang/wgsl/ast/identifier.h"
#include "src/tint/lang/wgsl/ast/module.h"
#include "src/tint/lang/wgsl/sem/function.h"

ShaderManager* ShaderManager::m_Instance;

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

ShaderReflectionInfo ReflectShader(Ref<Shader> shader) {
  ShaderReflectionInfo reflectionInfo;
  tint::wgsl::reader::Options options;
  options.allowed_features = {};

  tint::Source::File* file = new tint::Source::File(shader->GetName(), shader->GetSource());
  tint::Program program = tint::wgsl::reader::Parse(file, options);
  tint::inspector::Inspector inspector(program);

  std::unordered_map<uint32_t, std::unordered_map<uint32_t, tint::inspector::ResourceBinding>> inspectorResourceBindings;

  auto& resourceBindings = reflectionInfo.ResourceDeclarations;

	// TODO: Check shader is valid
  for (auto& entryPoint : inspector.GetEntryPoints()) {
    auto textureBindings = inspector.GetSampledTextureResourceBindings(entryPoint.name);
    auto depthTextureBindings = inspector.GetDepthTextureResourceBindings(entryPoint.name);

    for (auto& entry : textureBindings) {
      inspectorResourceBindings[entry.bind_group][entry.binding] = entry;
    }
    for (auto& entry : depthTextureBindings) {
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

      ResourceDeclaration info;
      info.Type = BindingType::UniformBindingType;
      info.Name = var->Declaration()->name->symbol.Name();
      info.GroupIndex = binding_info.group;
      info.LocationIndex = binding_info.binding;
      info.Size = unwrapped_type->Size();


			if(resourceBindings[info.GroupIndex].size() == 0)
				resourceBindings[info.GroupIndex].push_back(info);
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
          groupEntry.buffer.hasDynamicOffset = false;
          groupEntry.buffer.nextInChain = nullptr;
          groupEntry.buffer.minBindingSize = 0;
          groupEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex;
          break;
        case TextureBindingType:
          groupEntry.texture.sampleType = GetSampleType(inspectorResourceBindings[entry.GroupIndex][entry.LocationIndex].sampled_kind);
          groupEntry.texture.viewDimension = GetDimensionType(inspectorResourceBindings[entry.GroupIndex][entry.LocationIndex].dim);
          groupEntry.visibility = WGPUShaderStage_Fragment;
          break;
        case TextureDepthBindingType:
          groupEntry.texture.sampleType = WGPUTextureSampleType_Depth;
          groupEntry.texture.viewDimension = GetDimensionType(inspectorResourceBindings[entry.GroupIndex][entry.LocationIndex].dim);
          groupEntry.visibility = WGPUShaderStage_Fragment;
          break;
        case SamplerBindingType:
          groupEntry.sampler.type = WGPUSamplerBindingType_Filtering;
          groupEntry.visibility = WGPUShaderStage_Fragment;
          break;
        case CompareSamplerBindingType:
          groupEntry.sampler.type = WGPUSamplerBindingType_Comparison;
          groupEntry.visibility = WGPUShaderStage_Fragment;
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
