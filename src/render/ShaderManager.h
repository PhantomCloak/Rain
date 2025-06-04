#pragma once
#include <webgpu/webgpu.h>
#include <map>
#include <string>
#include "render/Shader.h"

namespace Rain {
  class ShaderManager {
   public:
    static Ref<Shader> LoadShader(const std::string& shaderId, const std::string& shaderPath);
    static Ref<Shader> GetShader(const std::string& shaderId);

    // TODO: Make better API
   private:
    static std::map<std::string, Ref<Shader>> m_Shaders;
  };
}  // namespace Rain
