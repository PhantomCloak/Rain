#include "ShaderManager.h"
#include <iostream>
#include "io/filesystem.h"

void ShaderManager::LoadShader(const std::string& shaderId,
                               const std::string& shaderPath) {
  std::string srcShader = FileSys::ReadFile(shaderPath);

  WGPUShaderModuleWGSLDescriptor shaderCodeDesc;
  shaderCodeDesc.chain.next = nullptr;
  shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
  shaderCodeDesc.code = srcShader.c_str();

  WGPUShaderModuleDescriptor shaderDesc;
  shaderDesc.nextInChain = &shaderCodeDesc.chain;

  WGPUShaderModule module = wgpuDeviceCreateShaderModule(_device, &shaderDesc);
  _shaders.emplace(shaderId, module);
};

WGPUShaderModule ShaderManager::GetShader(const std::string& shaderId) {
  if (_shaders.find(shaderId) == _shaders.end()) {
    std::cout << "cannot find the shader";
    return nullptr;
  }

  return _shaders[shaderId];
}
