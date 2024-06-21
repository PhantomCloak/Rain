#pragma once
#include "render/Shader.h"
#include <webgpu/webgpu.h>
#include <string>
#include <map>

class ShaderManager {
 public:
	ShaderManager() {
		m_Instance = this;
	};
  Ref<Shader> LoadShader(const std::string& shaderId, const std::string& shaderPath);
  Ref<Shader> GetShader(const std::string& shaderId);

	// TODO: Make better API
	static ShaderManager* Get() { return m_Instance; }

 private:
	static ShaderManager* m_Instance;
  std::map<std::string, Ref<Shader>> m_Shaders;
};
