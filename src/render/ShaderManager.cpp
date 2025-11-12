#include "ShaderManager.h"
#include <iostream>
#include "render/RenderUtils.h"

#include "render/RenderContext.h"
#include "src/tint/lang/core/type/array.h"
#include "src/tint/lang/core/type/i32.h"
#include "src/tint/lang/core/type/u32.h"
#include "src/tint/lang/core/type/f32.h"
#include "src/tint/lang/core/type/matrix.h"
#include "src/tint/lang/core/type/texture.h"
#include "src/tint/lang/core/type/storage_texture.h"
#include "src/tint/lang/core/type/sampled_texture.h"
#include "src/tint/lang/core/type/multisampled_texture.h"
#include "src/tint/lang/wgsl/inspector/inspector.h"
#include "src/tint/lang/wgsl/reader/reader.h"
#include "src/tint/lang/core/type/depth_texture.h"
#include "src/tint/lang/wgsl/ast/identifier.h"
#include "src/tint/lang/wgsl/ast/module.h"
#include "src/tint/lang/wgsl/sem/function.h"

namespace Rain
{
  std::map<std::string, Ref<Shader>> ShaderManager::m_Shaders;
  WGPUTextureViewDimension GetDimensionType(tint::core::type::TextureDimension type)
  {
    switch (type)
    {
      case tint::core::type::TextureDimension::k1d:
        return WGPUTextureViewDimension_1D;
      case tint::core::type::TextureDimension::k2d:
        return WGPUTextureViewDimension_2D;
      case tint::core::type::TextureDimension::k2dArray:
        return WGPUTextureViewDimension_2DArray;
      case tint::core::type::TextureDimension::k3d:
        return WGPUTextureViewDimension_3D;
      case tint::core::type::TextureDimension::kCube:
        return WGPUTextureViewDimension_Cube;
      case tint::core::type::TextureDimension::kCubeArray:
        return WGPUTextureViewDimension_CubeArray;
      case tint::core::type::TextureDimension::kNone:
        return WGPUTextureViewDimension_Undefined;
        break;
    }
  }

  static void GetTextureReflectionInfo(const tint::core::type::Texture* tex, WGPUTextureFormat& outFormat, WGPUTextureViewDimension& outDimension, WGPUTextureSampleType& outSampleType, BindingType& outTextureBindingType)
  {
    outFormat = WGPUTextureFormat_Undefined;
    outDimension = WGPUTextureViewDimension_Undefined;
    outSampleType = WGPUTextureSampleType_Undefined;
    outTextureBindingType = BindingType::TextureBindingType;

    if (!tex)
    {
      return;
    }

    if (const auto* st = tex->As<tint::core::type::StorageTexture>())
    {
      switch (st->TexelFormat())
      {
        case tint::core::TexelFormat::kRgba8Unorm:
          outFormat = WGPUTextureFormat_RGBA8Unorm;
          break;
        case tint::core::TexelFormat::kRgba16Float:
          outFormat = WGPUTextureFormat_RGBA16Float;
          break;
        case tint::core::TexelFormat::kRgba32Float:
          outFormat = WGPUTextureFormat_RGBA32Float;
          break;
        default:
          break;
      }
    }

    outDimension = GetDimensionType(tex->Dim());

    const tint::core::type::Type* BaseSampledTypePtr = nullptr;
    if (const auto* sampledTexture = tex->As<tint::core::type::SampledTexture>())
    {
      BaseSampledTypePtr = sampledTexture->Type();
    }
    else if (const auto* multisampledTexture = tex->As<tint::core::type::MultisampledTexture>())
    {
      BaseSampledTypePtr = multisampledTexture->Type();
    }
    else if (const auto* storageTexture = tex->As<tint::core::type::StorageTexture>())
    {
      BaseSampledTypePtr = storageTexture->Type();
      outTextureBindingType = BindingType::StorageBindingType;
    }
    else if (const auto* depthTexture = tex->As<tint::core::type::DepthTexture>())
    {
      outTextureBindingType = BindingType::TextureDepthBindingType;
    }

    if (BaseSampledTypePtr)
    {
      if (const auto* a = BaseSampledTypePtr->As<tint::core::type::Array>())
      {
        BaseSampledTypePtr = a->ElemType();
      }
      else if (const auto* mat = BaseSampledTypePtr->As<tint::core::type::Matrix>())
      {
        BaseSampledTypePtr = mat->Type();
      }
      else if (const auto* v = BaseSampledTypePtr->As<tint::core::type::Vector>())
      {
        BaseSampledTypePtr = v->Type();
      }

      if (BaseSampledTypePtr->Is<tint::core::type::F32>())
      {
        outSampleType = WGPUTextureSampleType_Float;
      }
      else if (BaseSampledTypePtr->Is<tint::core::type::U32>())
      {
        outSampleType = WGPUTextureSampleType_Uint;
      }
      else if (BaseSampledTypePtr->Is<tint::core::type::I32>())
      {
        outSampleType = WGPUTextureSampleType_Sint;
      }
    }
  }

  ShaderReflectionInfo ReflectShader(Ref<Shader> shader)
  {
    ShaderReflectionInfo reflectionInfo;

    tint::wgsl::reader::Options options;
    options.allowed_features = {};

    tint::Source::File* file = new tint::Source::File(shader->GetName(), shader->GetSource());
    tint::Program program = tint::wgsl::reader::Parse(file, options);

    if (program.Diagnostics().ContainsErrors())
    {
      RN_LOG_ERR("Shader Compilation Error: {0}", program.Diagnostics().Str());
      return reflectionInfo;
    }

    tint::inspector::Inspector inspector(program);

    auto& uniformTypes = reflectionInfo.UniformTypes;
    auto& resourceBindings = reflectionInfo.ResourceDeclarations;

    auto alreadyAdded = [&](uint32_t group, uint32_t binding, std::string_view name)
    {
      for (auto& r : resourceBindings[group])
      {
        if (r.LocationIndex == binding && r.Name == name)
        {
          return true;
        }
      }
      return false;
    };

    for (const auto& entryPoint : inspector.GetEntryPoints())
    {
      auto* func = program.AST().Functions().Find(program.Symbols().Get(entryPoint.name));
      auto* func_sem = program.Sem().Get(func);
      if (!func_sem)
      {
        continue;
      }

      for (const auto* shaderResource : func_sem->TransitivelyReferencedGlobals())
      {
        const auto& bp_opt = shaderResource->Attributes().binding_point;
        if (!bp_opt.has_value())
        {
          continue;
        }

        const auto bp = *bp_opt;
        const auto* unwrapped = shaderResource->Type()->UnwrapRef();

        ResourceDeclaration info{};
        info.Name = shaderResource->Declaration()->name->symbol.Name();
        info.GroupIndex = bp.group;
        info.LocationIndex = bp.binding;
        info.Size = unwrapped->Size();
        info.SampleType = WGPUTextureSampleType_Undefined;
        info.ViewDimension = WGPUTextureViewDimension_Undefined;
        info.ImageFormat = WGPUTextureFormat_Undefined;

        if (shaderResource->AddressSpace() == tint::core::AddressSpace::kUniform)
        {
          if (const auto* s = unwrapped->As<tint::core::type::Struct>())
          {
            const auto symbolName = s->Name().Name();
            if (uniformTypes.find(symbolName) == uniformTypes.end())
            {
              for (const auto* m : s->Members())
              {
                auto name = m->Name().Name();
                uniformTypes[symbolName][name] = {
                    .Name = name,
                    .Type = ShaderUniformType::Int,
                    .Size = m->Size(),
                    .Offset = m->Offset()};
              }
            }
            info.Type = BindingType::UniformBindingType;
            if (!alreadyAdded(info.GroupIndex, info.LocationIndex, info.Name))
            {
              resourceBindings[info.GroupIndex].push_back(info);
            }
            continue;
          }
        }

        if (const auto* Texture = unwrapped->As<tint::core::type::Texture>())
        {
          GetTextureReflectionInfo(Texture, info.ImageFormat, info.ViewDimension, info.SampleType, info.Type);

          if (!alreadyAdded(info.GroupIndex, info.LocationIndex, info.Name))
          {
            resourceBindings[info.GroupIndex].push_back(info);
          }
          continue;
        }

        if (const auto* samp = unwrapped->As<tint::core::type::Sampler>())
        {
          info.Type = (samp->Kind() == tint::core::type::SamplerKind::kComparisonSampler)
                          ? BindingType::CompareSamplerBindingType
                          : BindingType::SamplerBindingType;
          if (!alreadyAdded(info.GroupIndex, info.LocationIndex, info.Name))
          {
            resourceBindings[info.GroupIndex].push_back(info);
          }
        }
      }
    }

    for (auto& [groupIndex, entries] : resourceBindings)
    {
      std::vector<WGPUBindGroupLayoutEntry> layoutEntries;
      std::sort(entries.begin(), entries.end(), [&](const ResourceDeclaration& current, const ResourceDeclaration& second)
                { return current.LocationIndex < second.LocationIndex; });

      for (const auto& entry : entries)
      {
        WGPUBindGroupLayoutEntry groupEntry = {};
        groupEntry.binding = entry.LocationIndex;
        groupEntry.nextInChain = nullptr;

        switch (entry.Type)
        {
          case UniformBindingType:
            groupEntry.buffer.type = WGPUBufferBindingType_Uniform;

            groupEntry.buffer.hasDynamicOffset = !entry.Name.empty() && entry.Name[0] == 'u' && entry.Name[1] == 'd';
            groupEntry.buffer.nextInChain = nullptr;
            groupEntry.buffer.minBindingSize = 0;

            groupEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Vertex | WGPUShaderStage_Compute;
            break;
          case TextureBindingType:
            groupEntry.texture.sampleType = entry.SampleType;
            groupEntry.texture.viewDimension = entry.ViewDimension;
            groupEntry.visibility = WGPUShaderStage_Fragment | WGPUShaderStage_Compute;
            break;
          case TextureDepthBindingType:
            groupEntry.texture.sampleType = WGPUTextureSampleType_Depth;
            groupEntry.texture.viewDimension = entry.ViewDimension;
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
            groupEntry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
            groupEntry.storageTexture.format = entry.ImageFormat;
            groupEntry.storageTexture.viewDimension = entry.ViewDimension;

            groupEntry.visibility = WGPUShaderStage_Compute;
            break;
        }

        layoutEntries.push_back(groupEntry);
      }

      std::string label = shader->GetName() + std::to_string(groupIndex);

      WGPUBindGroupLayoutDescriptor layoutDesc = {};
      layoutDesc.nextInChain = nullptr;
      layoutDesc.label = RenderUtils::MakeLabel(label);
      layoutDesc.entries = layoutEntries.data();
      layoutDesc.entryCount = layoutEntries.size();

      WGPUBindGroupLayout layout = wgpuDeviceCreateBindGroupLayout(RenderContext::GetDevice(), &layoutDesc);
      reflectionInfo.LayoutDescriptors[groupIndex] = layout;
    }

    return reflectionInfo;
  }

  Ref<Shader> ShaderManager::LoadShader(const std::string& shaderId,
                                        const std::string& shaderPath)
  {
    Ref<Shader> shader = Shader::Create(shaderId, shaderPath);
    auto reflectionInfo = ReflectShader(shader);

    shader->SetReflectionInfo(reflectionInfo);
    m_Shaders[shaderId] = shader;

    return shader;
  };

  Ref<Shader> ShaderManager::LoadShaderFromString(const std::string& shaderId,
                                                  const std::string& shaderStr)
  {
    Ref<Shader> shader = Shader::CreateFromSring(shaderId, shaderStr);
    auto reflectionInfo = ReflectShader(shader);

    shader->SetReflectionInfo(reflectionInfo);
    m_Shaders[shaderId] = shader;

    return shader;
  };

  Ref<Shader> ShaderManager::GetShader(const std::string& shaderId)
  {
    if (m_Shaders.find(shaderId) == m_Shaders.end())
    {
      std::cout << "cannot find the shader";
      return nullptr;
    }

    return m_Shaders[shaderId];
  }
}  // namespace Rain
