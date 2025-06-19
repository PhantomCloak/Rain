#pragma once

#include <webgpu/webgpu.h>
#include <cstdint>
#include <set>
#include <string>
#include "render/Sampler.h"
#include "render/Texture.h"

namespace Rain {
  enum class ShaderDataType {
    None = 0,
    Float,
    Float2,
    Float3,
    Float4,
    Mat3,
    Mat4,
    Int,
    Int2,
    Int3,
    Int4,
    Bool
  };

  static uint32_t ShaderDataTypeSize(ShaderDataType type) {
    switch (type) {
      case ShaderDataType::Float:
        return 4;
      case ShaderDataType::Float2:
        return 4 * 2;
      case ShaderDataType::Float3:
        return 4 * 3;
      case ShaderDataType::Float4:
        return 4 * 4;
      case ShaderDataType::Mat3:
        return 4 * 3 * 3;
      case ShaderDataType::Mat4:
        return 4 * 4 * 4;
      case ShaderDataType::Int:
        return 4;
      case ShaderDataType::Int2:
        return 4 * 2;
      case ShaderDataType::Int3:
        return 4 * 3;
      case ShaderDataType::Int4:
        return 4 * 4;
      case ShaderDataType::Bool:
        return 1;
      case ShaderDataType::None:
        break;
    }

    // TOOD Assert
    return 0;
  }

  static uint32_t ShaderDataTypeAlignment(ShaderDataType type) {
    switch (type) {
      case ShaderDataType::Float:
        return 4;
      case ShaderDataType::Float2:
        return 4 * 2;
      case ShaderDataType::Float3:
        return 4 * 3;
      case ShaderDataType::Float4:
        return 4 * 4;
      case ShaderDataType::Mat3:
        return 4 * 3 * 3;
      case ShaderDataType::Mat4:
        return 4 * 4 * 4;
      case ShaderDataType::Int:
        return 4;
      case ShaderDataType::Int2:
        return 4 * 2;
      case ShaderDataType::Int3:
        return 4 * 3;
      case ShaderDataType::Int4:
        return 4 * 4;
      case ShaderDataType::Bool:
        return 1;
      case ShaderDataType::None:
        break;
    }

    // TOOD Assert
    return 0;
  }

  struct BufferElement {
    std::string Name;
    ShaderDataType Type;
    uint32_t Location;
    uint32_t Size;
    size_t Offset;

    BufferElement() = default;

    // Since we have no shader translation stuff at the moment thus we cannot get locations / types from shader
    // We have to supply them manually
    BufferElement(uint32_t location, ShaderDataType type, const std::string& name, uint32_t offset)
        : Name(name), Type(type), Location(location), Size(ShaderDataTypeSize(type)), Offset(offset) {
    }

    BufferElement(ShaderDataType type, const std::string& name)
        : Name(name), Type(type), Location(0), Size(ShaderDataTypeSize(type)), Offset(0) {
    }

    uint32_t GetComponentCount() const {
      switch (Type) {
        case ShaderDataType::Float:
          return 1;
        case ShaderDataType::Float2:
          return 2;
        case ShaderDataType::Float3:
          return 3;
        case ShaderDataType::Float4:
          return 4;
        case ShaderDataType::Mat3:
          return 3;  // 3* float3
        case ShaderDataType::Mat4:
          return 4;  // 4* float4
        case ShaderDataType::Int:
          return 1;
        case ShaderDataType::Int2:
          return 2;
        case ShaderDataType::Int3:
          return 3;
        case ShaderDataType::Int4:
          return 4;
        case ShaderDataType::Bool:
          return 1;
        case ShaderDataType::None:
          break;
      }

      // TOOD Assert
      return 0;
    }
  };

  class VertexBufferLayout {
   public:
    VertexBufferLayout()
        : m_Stride(0), m_Elements() {};

    VertexBufferLayout(uint32_t stride, std::initializer_list<BufferElement> elements)
        : m_Stride(stride), m_Elements(elements) {
    }

    uint32_t GetStride() const {
      return m_Stride;
    }

    int GetElementCount() const {
      return m_Elements.size();
    }

    const std::vector<BufferElement>& GetElements() const {
      return m_Elements;
    }

    std::vector<BufferElement>::iterator begin() {
      return m_Elements.begin();
    }
    std::vector<BufferElement>::iterator end() {
      return m_Elements.end();
    }
    std::vector<BufferElement>::const_iterator begin() const {
      return m_Elements.begin();
    }
    std::vector<BufferElement>::const_iterator end() const {
      return m_Elements.end();
    }

   private:
    std::vector<BufferElement> m_Elements;
    uint32_t m_Stride = 0;
  };

  enum class GroupLayoutVisibility {
    Vertex,
    Fragment,
    Both
  };

  enum class GroupLayoutType {
    Uniform,
    UniformDynamic,
    Storage,
    StorageReadOnly,
    StorageReadOnlyDynamic,
    Sampler,
    SamplerCompare,
    Texture,
    TextureDepth
  };

  struct LayoutElement {
    int Binding;
    GroupLayoutVisibility Visiblity;
    GroupLayoutType Type;

    LayoutElement() = default;

    LayoutElement(int binding, GroupLayoutVisibility visibility, GroupLayoutType type = GroupLayoutType::Uniform)
        : Binding(binding), Visiblity(visibility), Type(type) {
    }
  };

  class GroupLayout {
   public:
    GroupLayout() {
    }

    GroupLayout(std::initializer_list<LayoutElement> elements)
        : m_Elements(elements) {
      CalculateLayoutCount();
    }

    void CalculateLayoutCount() {
      std::set<int> distinctGroups;
      for (auto element : m_Elements) {
        distinctGroups.insert(element.Binding);
      }
      m_LayoutCount = distinctGroups.size();
    }

    const int LayoutCount() const {
      return m_LayoutCount;
    };

    const int GetElementCount() const {
      return m_Elements.size();
    }

    const std::vector<LayoutElement>& GetElements() const {
      return m_Elements;
    }

    std::vector<LayoutElement>::iterator begin() {
      return m_Elements.begin();
    }
    std::vector<LayoutElement>::iterator end() {
      return m_Elements.end();
    }
    std::vector<LayoutElement>::const_iterator begin() const {
      return m_Elements.begin();
    }
    std::vector<LayoutElement>::const_iterator end() const {
      return m_Elements.end();
    }

   private:
    int m_LayoutCount = 0;
    std::vector<LayoutElement> m_Elements;
  };

  class LayoutUtils {
   public:
    static void SetVisibility(WGPUBindGroupLayoutEntry& entry, GroupLayoutVisibility visibility);
    static void SetType(WGPUBindGroupLayoutEntry& entry, GroupLayoutType type);
    static std::vector<WGPUBindGroupLayoutEntry> ParseGroupLayout(GroupLayout layout);
    static WGPUBindGroupLayout CreateBindGroup(std::string label, WGPUDevice device, GroupLayout layout);
    static uint32_t CeilToNextMultiple(uint32_t value, uint32_t step);
  };

  class RenderTypeUtils {
   public:
    static TextureFormat FromRenderType(WGPUTextureFormat format);
    static TextureWrappingFormat FromRenderType(WGPUAddressMode format);
    static FilterMode FromRenderType(WGPUFilterMode format);

    static WGPUTextureFormat ToRenderType(TextureFormat format);
    static WGPUAddressMode ToRenderType(TextureWrappingFormat format);
    static WGPUFilterMode ToRenderType(FilterMode format);
  };

  class RenderUtils {
   public:
    static uint32_t CalculateMipCount(uint32_t width, uint32_t height);

#ifndef __EMSCRIPTEN__
    static WGPUStringView MakeLabel(const char* str);
    static WGPUStringView MakeLabel(const std::string& str);
#else
    static const char* MakeLabel(const char* str);
    static const char* MakeLabel(const std::string& str);
#endif
    const char* GetAdapterTypeString(WGPUAdapterType adapterType);
    const char* GetBackendTypeString(WGPUBackendType backendType);
  };

  class TextureUtils {
   public:
    static uint32_t GetBytesPerPixel(TextureFormat format);
  };
}  // namespace Rain
