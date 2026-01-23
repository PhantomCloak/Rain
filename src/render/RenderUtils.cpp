#include "RenderUtils.h"

namespace Rain
{
  void LayoutUtils::SetVisibility(WGPUBindGroupLayoutEntry& entry,
                                  GroupLayoutVisibility visibility)
  {
    switch (visibility)
    {
      case GroupLayoutVisibility::Vertex:
        entry.visibility = WGPUShaderStage_Vertex;
        break;
      case GroupLayoutVisibility::Fragment:
        entry.visibility = WGPUShaderStage_Fragment;
        break;
      case GroupLayoutVisibility::Both:
        entry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
        break;
    }
  }

  // For std::string if needed
#ifndef __EMSCRIPTEN__
  WGPUStringView RenderUtils::MakeLabel(const std::string& str)
  {
    return {str.c_str(), str.size()};
  }
  WGPUStringView RenderUtils::MakeLabel(const char* str)
  {
    return {str, strlen(str)};
  }
#else
  const char* RenderUtils::MakeLabel(const std::string& str)
  {
    return str.c_str();
  }
  const char* RenderUtils::MakeLabel(const char* str)
  {
    return str;
  }
#endif

  void LayoutUtils::SetType(WGPUBindGroupLayoutEntry& entry, GroupLayoutType type)
  {
    switch (type)
    {
      case GroupLayoutType::Uniform:
        entry.buffer.type = WGPUBufferBindingType_Uniform;
        break;
      case GroupLayoutType::Texture:
        entry.texture.sampleType = WGPUTextureSampleType_Float;
        entry.texture.viewDimension = WGPUTextureViewDimension_2D;
        break;
      case GroupLayoutType::TextureDepth:
        entry.texture.sampleType = WGPUTextureSampleType_Depth;
        entry.texture.viewDimension = WGPUTextureViewDimension_2D;
        break;
      case GroupLayoutType::Sampler:
        entry.sampler.type = WGPUSamplerBindingType_Filtering;
        break;
      case GroupLayoutType::SamplerCompare:
        entry.sampler.type = WGPUSamplerBindingType_Comparison;
        break;
      case GroupLayoutType::UniformDynamic:
        entry.buffer.type = WGPUBufferBindingType_Uniform;
        entry.buffer.hasDynamicOffset = true;
        break;
      case GroupLayoutType::Storage:
        entry.buffer.type = WGPUBufferBindingType_Storage;
        break;
      case GroupLayoutType::StorageReadOnly:
        entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
        break;
      case GroupLayoutType::StorageReadOnlyDynamic:
        entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
        entry.buffer.hasDynamicOffset = true;
        break;
    }
  }

  std::vector<WGPUBindGroupLayoutEntry> LayoutUtils::ParseGroupLayout(GroupLayout layout)
  {
    std::vector<WGPUBindGroupLayoutEntry> groups;
    int groupsMemberCtx = 0;

    // We may need to account for buffer.minBindingSize
    for (auto layoutItem : layout)
    {
      WGPUBindGroupLayoutEntry entry = {};

      entry.binding = groupsMemberCtx++;
      SetVisibility(entry, layoutItem.Visiblity);
      SetType(entry, layoutItem.Type);

      groups.push_back(entry);
    }

    return groups;
  }

  WGPUBindGroupLayout LayoutUtils::CreateBindGroup(std::string label, WGPUDevice device, GroupLayout layout)
  {
    std::vector<WGPUBindGroupLayoutEntry> entries = ParseGroupLayout(layout);

    // TODO(memory): We need to introduce custom allocator in the future
    // or alternatively use callbacks to handle release
    WGPUBindGroupLayoutDescriptor* descriptor = new WGPUBindGroupLayoutDescriptor{};
    descriptor->label = RenderUtils::MakeLabel(label);
    descriptor->entries = entries.data();
    descriptor->entryCount = entries.size();

    return wgpuDeviceCreateBindGroupLayout(device, descriptor);
  }

  uint32_t LayoutUtils::CeilToNextMultiple(uint32_t value, uint32_t step)
  {
    uint32_t divide_and_ceil = value / step + (value % step == 0 ? 0 : 1);
    return step * divide_and_ceil;
  }

  TextureFormat RenderTypeUtils::FromRenderType(WGPUTextureFormat format)
  {
    RN_ASSERT(false, "Haven't implemented yet");
  }

  TextureWrappingFormat RenderTypeUtils::FromRenderType(WGPUAddressMode format)
  {
    RN_ASSERT(false, "Haven't implemented yet");
  }

  FilterMode RenderTypeUtils::FromRenderType(WGPUFilterMode format)
  {
    RN_ASSERT(false, "Haven't implemented yet");
  }

  WGPUTextureFormat RenderTypeUtils::ToRenderType(TextureFormat format)
  {
    switch (format)
    {
      case RGBA8:
        return WGPUTextureFormat_RGBA8Unorm;
      case BRGBA8:
        return WGPUTextureFormat_BGRA8Unorm;
      case Depth24Plus:
        return WGPUTextureFormat_Depth24Plus;
      case RGBA16F:
        return WGPUTextureFormat_RGBA16Float;
      case RGBA32F:
        return WGPUTextureFormat_RGBA32Float;
      case Undefined:
        return WGPUTextureFormat_Undefined;
      default:
        RN_ASSERT(false, "Format conversion for TextureFormat not exist.")
    };
  }

  WGPUAddressMode RenderTypeUtils::ToRenderType(TextureWrappingFormat format)
  {
    switch (format)
    {
      case ClampToEdges:
        return WGPUAddressMode_ClampToEdge;
      case Repeat:
        return WGPUAddressMode_Repeat;
        break;
      default:
        RN_ASSERT(false, "Format conversion for TextureWrappingFormat not exist.")
    }
  }
  WGPUFilterMode RenderTypeUtils::ToRenderType(FilterMode format)
  {
    switch (format)
    {
      case Nearest:
        return WGPUFilterMode_Nearest;
      case Linear:
        return WGPUFilterMode_Linear;
      default:
        RN_ASSERT(false, "Format conversion for FilterMode not exist.")
    }
  }

  uint32_t RenderUtils::CalculateMipCount(uint32_t width, uint32_t height)
  {
    return (uint32_t)(floor((float)(log2(glm::max(width, height))))) + 1;
  }

  const char* RenderUtils::GetAdapterTypeString(WGPUAdapterType adapterType)
  {
    switch (adapterType)
    {
      case WGPUAdapterType_DiscreteGPU:
        return "Discrete GPU";
      case WGPUAdapterType_IntegratedGPU:
        return "Integrated GPU";
      case WGPUAdapterType_CPU:
        return "CPU";
      case WGPUAdapterType_Unknown:
        return "Unknown";
      default:
        return "Unknown";
    }
  }

  const char* RenderUtils::GetBackendTypeString(WGPUBackendType backendType)
  {
    switch (backendType)
    {
      case WGPUBackendType_Null:
        return "Null";
      case WGPUBackendType_WebGPU:
        return "WebGPU";
      case WGPUBackendType_D3D11:
        return "Direct3D 11";
      case WGPUBackendType_D3D12:
        return "Direct3D 12";
      case WGPUBackendType_Metal:
        return "Metal";
      case WGPUBackendType_Vulkan:
        return "Vulkan";
      case WGPUBackendType_OpenGL:
        return "OpenGL";
      case WGPUBackendType_OpenGLES:
        return "OpenGLES";
      default:
        return "Unknown";
    }
  }

  uint32_t TextureUtils::GetBytesPerPixel(TextureFormat format)
  {
    switch (format)
    {
      case TextureFormat::RGBA8:
        return 4;
      case TextureFormat::RGBA16F:
        return 8;

      case TextureFormat::RGBA32F:
        return 16;
      default:
        RN_LOG_ERR("GetBytesPerPixel: Unsupported format {}", (int)format);
        return 0;
    }
  }
}  // namespace Rain
