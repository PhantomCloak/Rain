#pragma once
#include <webgpu/webgpu.h>
#include <string>
#include <map>

class ShaderManager {
 public:
  ShaderManager(WGPUDevice& device)
      : _device(device) {
  }

  void LoadShader(const std::string& shaderId, const std::string& shaderPath);
  WGPUShaderModule GetShader(const std::string& shaderId);

 private:
  WGPUDevice& _device;
  std::map<std::string, WGPUShaderModule> _shaders;
};
