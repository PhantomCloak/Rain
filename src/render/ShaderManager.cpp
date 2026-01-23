#include "ShaderManager.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include "render/RenderUtils.h"

#include "render/RenderContext.h"
#include "render/Shader.h"

namespace Rain
{
  std::map<std::string, Ref<Shader>> ShaderManager::m_Shaders;

  // ============================================================================
  // Manual WGSL Parser
  // ============================================================================

  namespace WGSLParser
  {
    // WGSL type size and alignment info
    struct TypeInfo
    {
      uint32_t Size;
      uint32_t Align;
      ShaderUniformType UniformType;
    };

    // Parsed struct member
    struct StructMember
    {
      std::string Name;
      std::string TypeName;
      uint32_t Size;
      uint32_t Align;
      uint32_t Offset;
      ShaderUniformType UniformType;
    };

    // Parsed struct definition
    struct StructDef
    {
      std::string Name;
      std::vector<StructMember> Members;
      uint32_t Size;
      uint32_t Align;
    };

    // Parsed binding declaration
    struct BindingDecl
    {
      uint32_t Group;
      uint32_t Binding;
      std::string VarName;
      std::string TypeName;
      bool IsUniform;
      bool IsStorage;
    };

    // Trim whitespace from both ends
    static std::string Trim(const std::string& str)
    {
      size_t start = str.find_first_not_of(" \t\r\n");
      if (start == std::string::npos)
      {
        return "";
      }
      size_t end = str.find_last_not_of(" \t\r\n");
      return str.substr(start, end - start + 1);
    }

    // Round up to alignment
    static uint32_t AlignUp(uint32_t value, uint32_t align)
    {
      return (value + align - 1) & ~(align - 1);
    }

    // Get size/alignment for primitive and built-in WGSL types
    static bool GetBuiltinTypeInfo(const std::string& typeName, TypeInfo& outInfo)
    {
      // Remove all spaces for comparison
      std::string type = typeName;
      type.erase(std::remove_if(type.begin(), type.end(), ::isspace), type.end());

      // Scalars
      if (type == "f32")
      {
        outInfo = {4, 4, ShaderUniformType::Float};
        return true;
      }
      if (type == "i32")
      {
        outInfo = {4, 4, ShaderUniformType::Int};
        return true;
      }
      if (type == "u32")
      {
        outInfo = {4, 4, ShaderUniformType::Int};
        return true;
      }
      if (type == "bool")
      {
        outInfo = {4, 4, ShaderUniformType::Bool};
        return true;
      }

      // Vector types - support both vec2<f32> and vec2f syntax
      if (type == "vec2<f32>" || type == "vec2f" || type == "vec2<i32>" || type == "vec2<u32>")
      {
        outInfo = {8, 8, ShaderUniformType::Vec2};
        return true;
      }
      if (type == "vec3<f32>" || type == "vec3f" || type == "vec3<i32>" || type == "vec3<u32>")
      {
        outInfo = {12, 16, ShaderUniformType::Vec3};
        return true;
      }
      if (type == "vec4<f32>" || type == "vec4f" || type == "vec4<i32>" || type == "vec4<u32>")
      {
        outInfo = {16, 16, ShaderUniformType::Vec4};
        return true;
      }

      // Matrix types - column-major, each column is a vec
      if (type == "mat2x2<f32>" || type == "mat2x2f")
      {
        outInfo = {16, 8, ShaderUniformType::Float};  // 2 columns of vec2
        return true;
      }
      if (type == "mat3x3<f32>" || type == "mat3x3f")
      {
        outInfo = {48, 16, ShaderUniformType::Float};  // 3 columns of vec3 (each padded to 16)
        return true;
      }
      if (type == "mat4x4<f32>" || type == "mat4x4f")
      {
        outInfo = {64, 16, ShaderUniformType::Float};  // 4 columns of vec4
        return true;
      }
      if (type == "mat2x3<f32>" || type == "mat2x3f")
      {
        outInfo = {32, 16, ShaderUniformType::Float};  // 2 columns of vec3
        return true;
      }
      if (type == "mat2x4<f32>" || type == "mat2x4f")
      {
        outInfo = {32, 16, ShaderUniformType::Float};  // 2 columns of vec4
        return true;
      }
      if (type == "mat3x2<f32>" || type == "mat3x2f")
      {
        outInfo = {24, 8, ShaderUniformType::Float};  // 3 columns of vec2
        return true;
      }
      if (type == "mat3x4<f32>" || type == "mat3x4f")
      {
        outInfo = {48, 16, ShaderUniformType::Float};  // 3 columns of vec4
        return true;
      }
      if (type == "mat4x2<f32>" || type == "mat4x2f")
      {
        outInfo = {32, 8, ShaderUniformType::Float};  // 4 columns of vec2
        return true;
      }
      if (type == "mat4x3<f32>" || type == "mat4x3f")
      {
        outInfo = {64, 16, ShaderUniformType::Float};  // 4 columns of vec3
        return true;
      }

      return false;
    }

    // Parse array type: array<T, N> -> returns element type and count
    static bool ParseArrayType(const std::string& typeName, std::string& outElemType, int& outCount)
    {
      static const std::regex arrayRe(R"(array\s*<\s*(.+?)\s*,\s*(\d+)\s*>)");
      std::smatch match;
      if (std::regex_search(typeName, match, arrayRe))
      {
        outElemType = match[1].str();
        outCount = std::stoi(match[2].str());
        return true;
      }
      return false;
    }

    // Get texture view dimension from type name
    // Order matters: check more specific patterns first (e.g., 2d_array before 2d)
    static WGPUTextureViewDimension GetTextureDimension(const std::string& typeName)
    {
      if (typeName.find("texture_1d") != std::string::npos)
      {
        return WGPUTextureViewDimension_1D;
      }
      // Check 2d_array before 2d (matches texture_2d_array, texture_depth_2d_array, texture_storage_2d_array)
      if (typeName.find("_2d_array") != std::string::npos)
      {
        return WGPUTextureViewDimension_2DArray;
      }
      if (typeName.find("_2d") != std::string::npos)
      {
        return WGPUTextureViewDimension_2D;
      }
      if (typeName.find("texture_3d") != std::string::npos)
      {
        return WGPUTextureViewDimension_3D;
      }
      // Check cube_array before cube
      if (typeName.find("texture_cube_array") != std::string::npos)
      {
        return WGPUTextureViewDimension_CubeArray;
      }
      if (typeName.find("texture_cube") != std::string::npos)
      {
        return WGPUTextureViewDimension_Cube;
      }
      return WGPUTextureViewDimension_Undefined;
    }

    // Get texture sample type from type name
    static WGPUTextureSampleType GetTextureSampleType(const std::string& typeName)
    {
      if (typeName.find("<f32>") != std::string::npos)
      {
        return WGPUTextureSampleType_Float;
      }
      if (typeName.find("<i32>") != std::string::npos)
      {
        return WGPUTextureSampleType_Sint;
      }
      if (typeName.find("<u32>") != std::string::npos)
      {
        return WGPUTextureSampleType_Uint;
      }
      return WGPUTextureSampleType_Float;  // Default for unparameterized textures
    }

    // Determine binding type from WGSL type name
    static BindingType GetBindingType(const std::string& typeName, bool isUniform, bool isStorage)
    {
      if (isUniform)
      {
        return BindingType::UniformBindingType;
      }

      if (typeName.find("sampler_comparison") != std::string::npos)
      {
        return BindingType::CompareSamplerBindingType;
      }
      if (typeName.find("sampler") != std::string::npos)
      {
        return BindingType::SamplerBindingType;
      }
      if (typeName.find("texture_depth") != std::string::npos)
      {
        return BindingType::TextureDepthBindingType;
      }
      if (typeName.find("texture_storage") != std::string::npos || isStorage)
      {
        return BindingType::StorageBindingType;
      }
      if (typeName.find("texture") != std::string::npos)
      {
        return BindingType::TextureBindingType;
      }

      return BindingType::EmptyBinding;
    }

    // Get storage texture format from type
    static WGPUTextureFormat GetStorageTextureFormat(const std::string& typeName)
    {
      if (typeName.find("rgba8unorm") != std::string::npos)
      {
        return WGPUTextureFormat_RGBA8Unorm;
      }
      if (typeName.find("rgba8snorm") != std::string::npos)
      {
        return WGPUTextureFormat_RGBA8Snorm;
      }
      if (typeName.find("rgba16float") != std::string::npos)
      {
        return WGPUTextureFormat_RGBA16Float;
      }
      if (typeName.find("rgba32float") != std::string::npos)
      {
        return WGPUTextureFormat_RGBA32Float;
      }
      if (typeName.find("r32float") != std::string::npos)
      {
        return WGPUTextureFormat_R32Float;
      }
      if (typeName.find("rg32float") != std::string::npos)
      {
        return WGPUTextureFormat_RG32Float;
      }
      return WGPUTextureFormat_Undefined;
    }

    // Remove single-line comments from source
    static std::string RemoveComments(const std::string& src)
    {
      std::string result;
      std::istringstream iss(src);
      std::string line;

      while (std::getline(iss, line))
      {
        size_t commentPos = line.find("//");
        if (commentPos != std::string::npos)
        {
          line = line.substr(0, commentPos);
        }
        result += line + "\n";
      }

      // Remove block comments /* */
      static const std::regex blockCommentRe(R"(/\*[\s\S]*?\*/)");
      result = std::regex_replace(result, blockCommentRe, "");

      return result;
    }

    // Parse all struct definitions from shader source
    static std::map<std::string, StructDef> ParseStructs(const std::string& src,
                                                         std::map<std::string, std::map<std::string, ShaderTypeDecl>>& outShaderTypes)
    {
      std::map<std::string, StructDef> structs;

      // Match struct definitions: struct Name { ... };
      static const std::regex structRe(R"(struct\s+(\w+)\s*\{([^}]*)\})");
      std::sregex_iterator it(src.begin(), src.end(), structRe);
      std::sregex_iterator end;

      while (it != end)
      {
        std::smatch match = *it;
        StructDef def;
        def.Name = match[1].str();
        std::string body = match[2].str();

        // Parse members: name: type,
        // Match member: optional_attributes name: type,
        // Handles: @location(0) a_position: vec3f, or just memberName: type,
        static const std::regex memberRe(R"((?:@\w+\s*\([^)]*\)\s*)*(\w+)\s*:\s*([^,\n}]+))");
        std::sregex_iterator memberIt(body.begin(), body.end(), memberRe);

        uint32_t currentOffset = 0;
        uint32_t maxAlign = 4;

        while (memberIt != end)
        {
          std::smatch memberMatch = *memberIt;
          StructMember member;
          member.Name = Trim(memberMatch[1].str());
          member.TypeName = Trim(memberMatch[2].str());

          // Skip if this looks like an attribute (starts with @)
          if (member.Name.empty() || member.Name[0] == '@')
          {
            ++memberIt;
            continue;
          }

          TypeInfo typeInfo;
          if (GetBuiltinTypeInfo(member.TypeName, typeInfo))
          {
            member.Size = typeInfo.Size;
            member.Align = typeInfo.Align;
            member.UniformType = typeInfo.UniformType;
          }
          else
          {
            // Check for array type
            std::string elemType;
            int count;
            if (ParseArrayType(member.TypeName, elemType, count))
            {
              TypeInfo elemInfo;
              if (GetBuiltinTypeInfo(elemType, elemInfo))
              {
                // Array stride is element size rounded up to element alignment
                uint32_t stride = AlignUp(elemInfo.Size, elemInfo.Align);
                member.Size = stride * count;
                member.Align = elemInfo.Align;
                member.UniformType = elemInfo.UniformType;
              }
              else
              {
                // Array of structs - will be resolved in second pass
                member.Size = 0;
                member.Align = 16;
                member.UniformType = ShaderUniformType::None;
              }
            }
            else
            {
              // Unknown/struct type - will be resolved later
              member.Size = 0;
              member.Align = 16;
              member.UniformType = ShaderUniformType::None;
            }
          }

          // Apply alignment to current offset
          currentOffset = AlignUp(currentOffset, member.Align);
          member.Offset = currentOffset;
          currentOffset += member.Size;
          maxAlign = std::max(maxAlign, member.Align);

          def.Members.push_back(member);
          ++memberIt;
        }

        // Struct size is rounded up to struct alignment
        def.Align = maxAlign;
        def.Size = AlignUp(currentOffset, maxAlign);

        structs[def.Name] = def;
        ++it;
      }

      // Second pass: resolve struct references and populate outShaderTypes
      for (auto& [name, def] : structs)
      {
        uint32_t currentOffset = 0;
        uint32_t maxAlign = 4;

        for (auto& member : def.Members)
        {
          if (member.Size == 0)
          {
            // Try to resolve from parsed structs
            std::string elemType;
            int count;
            if (ParseArrayType(member.TypeName, elemType, count))
            {
              auto it = structs.find(elemType);
              if (it != structs.end())
              {
                uint32_t stride = AlignUp(it->second.Size, it->second.Align);
                member.Size = stride * count;
                member.Align = it->second.Align;
              }
            }
            else
            {
              auto it = structs.find(member.TypeName);
              if (it != structs.end())
              {
                member.Size = it->second.Size;
                member.Align = it->second.Align;
              }
            }
          }

          currentOffset = AlignUp(currentOffset, member.Align);
          member.Offset = currentOffset;
          currentOffset += member.Size;
          maxAlign = std::max(maxAlign, member.Align);

          // Populate ShaderTypes
          outShaderTypes[name][member.Name] = {
              .Name = member.Name,
              .Type = member.UniformType,
              .Size = member.Size,
              .Offset = member.Offset};
        }

        def.Size = AlignUp(currentOffset, maxAlign);
        def.Align = maxAlign;
      }

      return structs;
    }

    // Parse all @group/@binding declarations
    static std::vector<BindingDecl> ParseBindings(const std::string& src)
    {
      std::vector<BindingDecl> bindings;

      // Match: @group(X) @binding(Y) var<...> name: type;
      // or:    @group(X) @binding(Y) var name: type;
      static const std::regex bindingRe(
          R"(@group\s*\(\s*(\d+)\s*\)\s*@binding\s*\(\s*(\d+)\s*\)\s*var\s*(?:<\s*(\w+)\s*(?:,\s*\w+)?\s*>)?\s*(\w+)\s*:\s*([^;]+);)");

      std::sregex_iterator it(src.begin(), src.end(), bindingRe);
      std::sregex_iterator end;

      while (it != end)
      {
        std::smatch match = *it;
        BindingDecl decl;
        decl.Group = std::stoi(match[1].str());
        decl.Binding = std::stoi(match[2].str());

        std::string addressSpace = match[3].str();
        decl.IsUniform = (addressSpace == "uniform");
        decl.IsStorage = (addressSpace == "storage");

        decl.VarName = match[4].str();
        decl.TypeName = Trim(match[5].str());

        bindings.push_back(decl);
        ++it;
      }

      return bindings;
    }

  }  // namespace WGSLParser

  ShaderReflectionInfo ReflectShaderEx(Ref<Shader> shader)
  {
    using namespace WGSLParser;

    ShaderReflectionInfo reflectionInfo;
    std::string src = RemoveComments(shader->GetSource());

    // Parse struct definitions
    auto structs = ParseStructs(src, reflectionInfo.ShaderTypes);

    // Parse binding declarations
    auto bindings = ParseBindings(src);

    // Build ResourceDeclaration for each binding
    auto& resourceBindings = reflectionInfo.ShaderVariables;

    for (const auto& binding : bindings)
    {
      ResourceDeclaration info{};
      info.Name = binding.VarName;
      info.GroupIndex = binding.Group;
      info.LocationIndex = binding.Binding;
      info.Type = GetBindingType(binding.TypeName, binding.IsUniform, binding.IsStorage);
      info.SampleType = WGPUTextureSampleType_Undefined;
      info.ViewDimension = WGPUTextureViewDimension_Undefined;
      info.ImageFormat = WGPUTextureFormat_Undefined;
      info.Size = 0;

      if (binding.IsUniform)
      {
        // Look up struct size
        auto it = structs.find(binding.TypeName);
        if (it != structs.end())
        {
          info.Size = it->second.Size;
        }
        else
        {
          // Might be a primitive type
          TypeInfo typeInfo;
          if (GetBuiltinTypeInfo(binding.TypeName, typeInfo))
          {
            info.Size = typeInfo.Size;
          }
        }
      }
      else if (info.Type == BindingType::TextureBindingType ||
               info.Type == BindingType::TextureDepthBindingType)
      {
        info.ViewDimension = GetTextureDimension(binding.TypeName);
        info.SampleType = GetTextureSampleType(binding.TypeName);
      }
      else if (info.Type == BindingType::StorageBindingType)
      {
        info.ViewDimension = GetTextureDimension(binding.TypeName);
        info.ImageFormat = GetStorageTextureFormat(binding.TypeName);
      }

      resourceBindings[info.GroupIndex].push_back(info);
    }

    // Sort bindings within each group and create layouts
    for (auto& [groupIndex, entries] : resourceBindings)
    {
      std::sort(entries.begin(), entries.end(),
                [](const ResourceDeclaration& a, const ResourceDeclaration& b)
                { return a.LocationIndex < b.LocationIndex; });

      std::vector<WGPUBindGroupLayoutEntry> layoutEntries;

      for (const auto& entry : entries)
      {
        WGPUBindGroupLayoutEntry groupEntry = {};
        groupEntry.binding = entry.LocationIndex;
        groupEntry.nextInChain = nullptr;

        switch (entry.Type)
        {
          case UniformBindingType:
            groupEntry.buffer.type = WGPUBufferBindingType_Uniform;
            groupEntry.buffer.hasDynamicOffset = !entry.Name.empty() &&
                                                 entry.Name[0] == 'u' && entry.Name[1] == 'd';
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

          default:
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

      if (RenderContext::IsReady())
      {
        WGPUBindGroupLayout layout = wgpuDeviceCreateBindGroupLayout(RenderContext::GetDevice(), &layoutDesc);
        reflectionInfo.LayoutDescriptors[groupIndex] = layout;
      }
    }

    return reflectionInfo;
  }

  Ref<Shader> ShaderManager::LoadShader(const std::string& shaderId,
                                        const std::string& shaderPath)
  {
    Ref<Shader> shader = Shader::Create(shaderId, shaderPath);
    auto reflectionInfo = ReflectShaderEx(shader);

    shader->SetReflectionInfo(reflectionInfo);

    m_Shaders[shaderId] = shader;

    return shader;
  };

  Ref<Shader> ShaderManager::LoadShaderFromString(const std::string& shaderId,
                                                  const std::string& shaderStr)
  {
    Ref<Shader> shader = Shader::CreateFromSring(shaderId, shaderStr);
    auto reflectionInfo = ReflectShaderEx(shader);

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
