#include "ShaderManager.h"
#include <iostream>
#include "io/filesystem.h"

struct ShaderBundle {
  WGPUShaderModuleWGSLDescriptor shaderDescriptor;
	WGPUShaderModuleDescriptor shaderModuleDescriptor;
  WGPUShaderModule shaderModule;
};

void ShaderManager::LoadShader(const std::string& shaderId,
                               const std::string& shaderPath) {
  static std::map<std::string, ShaderBundle> shaderBundles;

  if (shaderBundles.find(shaderId) == shaderBundles.end()) {
    shaderBundles[shaderId] = {};
  }

  ShaderBundle& bundle = shaderBundles[shaderId];

  std::string srcShader = FileSys::ReadFile(shaderPath);

  WGPUShaderModuleWGSLDescriptor& shaderCodeDesc = bundle.shaderDescriptor;
  shaderCodeDesc.chain.next = nullptr;
  shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
  shaderCodeDesc.code = srcShader.c_str();

	WGPUShaderModuleDescriptor& shaderDesc = bundle.shaderModuleDescriptor;
	shaderDesc.label = shaderId.c_str();
  shaderDesc.nextInChain = &shaderCodeDesc.chain;

  bundle.shaderModule = wgpuDeviceCreateShaderModule(_device, &shaderDesc);
  _shaders.emplace(shaderId, bundle.shaderModule);
};

WGPUShaderModule ShaderManager::GetShader(const std::string& shaderId) {
  if (_shaders.find(shaderId) == _shaders.end()) {
    std::cout << "cannot find the shader";
    return nullptr;
  }

  return _shaders[shaderId];
}
