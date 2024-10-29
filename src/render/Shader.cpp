#include "Shader.h"
#include "render/RenderContext.h"
#include "io/filesystem.h"

Ref<Shader> Shader::Create(const std::string& name, const std::string& filePath) {
  std::string content = FileSys::ReadFile(filePath);
  return CreateFromSring(name, content);
}

Ref<Shader> Shader::CreateFromSring(const std::string& name, const std::string& content) {
  Ref<Shader> shader = CreateRef<Shader>(name, content);

  WGPUShaderModuleWGSLDescriptor shaderCodeDesc;
  shaderCodeDesc.chain.next = nullptr;
  shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
  shaderCodeDesc.code = content.c_str();
  WGPUShaderModuleDescriptor shaderDesc;
	shaderDesc.label = name.c_str();
  shaderDesc.nextInChain = &shaderCodeDesc.chain;

	shader->m_ShaderModule = wgpuDeviceCreateShaderModule(RenderContext::GetDevice(), &shaderDesc);
  return shader;
}
