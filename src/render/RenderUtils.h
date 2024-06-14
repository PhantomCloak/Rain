#pragma once

#include <cstdint>
#include <string>
#include <set>
#include <webgpu/webgpu.h>

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

struct BufferElement {
  std::string Name;
  ShaderDataType Type;
  uint32_t Size;
  size_t Offset;
  bool Normalized;

  BufferElement() = default;

  BufferElement(ShaderDataType type, const std::string& name, bool normalized = false)
      : Name(name), Type(type), Size(ShaderDataTypeSize(type)), Offset(0), Normalized(normalized) {
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

class BufferLayout {
 public:
  BufferLayout() {
  }

  BufferLayout(std::initializer_list<BufferElement> elements)
      : m_Elements(elements) {
    CalculateOffsetsAndStride();
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
  void CalculateOffsetsAndStride() {
    size_t offset = 0;
    m_Stride = 0;
    for (auto& element : m_Elements) {
      element.Offset = offset;
      offset += element.Size;
      m_Stride += element.Size;
    }
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
