#pragma once
#include <map>
#include <string>
#include <vector>
#include "core/Ref.h"
#include "webgpu/webgpu.h"

enum BindingType {
  UniformBindingType,
  TextureBindingType,
  StorageBindingType,
  TextureDepthBindingType,
  SamplerBindingType,
  CompareSamplerBindingType
};

enum ShaderUniformType {
  None = 0,
  Int,
  Float,
  Vec2,
  Vec3,
  Vec4,
  Bool
};

struct ShaderTypeDecl {
	std::string Name;
	ShaderUniformType Type = ShaderUniformType::None;
	uint32_t Size;
	uint32_t Offset;
};

struct ResourceDeclaration {
  std::string Name;
  BindingType Type;
  uint32_t GroupIndex;
  uint32_t LocationIndex;
  uint32_t Size;
};

struct TextureSpec {
  uint32_t SampleType;
  uint32_t DimensionType;
};

struct ShaderReflectionInfo {
  std::map<int, std::vector<ResourceDeclaration>> ResourceDeclarations;
  std::map<int, WGPUBindGroupLayout> LayoutDescriptors;
  std::map<std::string, std::map<std::string, ShaderTypeDecl>> UserTypes;
};

class Shader {
 public:
  const std::string& GetName() const { return m_Name; }
  const std::string& GetSource() const { return m_Content; }

	void Reload(std::string& content);

  static Ref<Shader> Create(const std::string& name, const std::string& filePath);
  static Ref<Shader> CreateFromSring(const std::string& name, const std::string& content);
  static WGPUBindGroupLayout GetLayout(uint32_t index);

  // Ref<T> cannot instantiate this class unless it's public
  Shader(const std::string& name, const std::string& content)
      : m_Name(name), m_Content(content) {}

  void SetReflectionInfo(const ShaderReflectionInfo& info) { m_ReflectionInfo = info; }
  ShaderReflectionInfo& GetReflectionInfo() { return m_ReflectionInfo; }

  const WGPUShaderModule GetNativeShaderModule() { return m_ShaderModule; }

 private:
  ShaderReflectionInfo m_ReflectionInfo;
  std::string m_Name;
  std::string m_Content;
  std::string m_Path;
  WGPUShaderModule m_ShaderModule;
};
