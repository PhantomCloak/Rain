#pragma once
#include "render/Shader.h"
#include <webgpu/webgpu.h>
#include <string>
#include <map>

class ShaderManager {
 public:
  static Ref<Shader> LoadShader(const std::string& shaderId, const std::string& shaderPath);
  static Ref<Shader> GetShader(const std::string& shaderId);

	// TODO: Make better API
 private:
  static std::map<std::string, Ref<Shader>> m_Shaders;
};
