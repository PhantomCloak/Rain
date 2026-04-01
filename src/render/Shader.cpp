#include "Shader.h"
#include "io/filesystem.h"
#include "render/RenderContext.h"
#include "render/RenderUtils.h"
#include "render/WGSLPreprocessor.h"

namespace WebEngine
{
  Ref<Shader> Shader::Create(const std::string& name, const std::string& filePath)
  {
    if (!FileSys::IsFileExist(filePath))
    {
      RN_LOG_ERR("Shader Creation Failed: File '{0}' does not exist", filePath);
      return nullptr;
    }

    std::string content = FileSys::ReadFile(filePath);
    return CreateFromSring(name, content);
  }

  Ref<Shader> Shader::CreateFromSring(const std::string& name, const std::string& content)
  {
    std::string processed = WGSLPreprocessor::Process(content);
    Ref<Shader> shader = CreateRef<Shader>(name, processed);

#ifndef __EMSCRIPTEN__
    WGPUShaderSourceWGSL shaderCodeDesc;
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    shaderCodeDesc.code = RenderUtils::MakeLabel(processed);
#else
    WGPUShaderModuleWGSLDescriptor shaderCodeDesc;
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    shaderCodeDesc.code = RenderUtils::MakeLabel(processed);
#endif
    WGPUShaderModuleDescriptor shaderDesc;
    shaderDesc.label = RenderUtils::MakeLabel(name);
    shaderDesc.nextInChain = &shaderCodeDesc.chain;

    if (RenderContext::IsReady())
    {
      shader->m_ShaderModule = wgpuDeviceCreateShaderModule(RenderContext::GetDevice(), &shaderDesc);
    }
    return shader;
  }

  void Shader::Reload(std::string& content)
  {
    std::string processed = WGSLPreprocessor::Process(content);
#ifndef __EMSCRIPTEN__
    WGPUShaderSourceWGSL shaderCodeDesc;
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    shaderCodeDesc.code = RenderUtils::MakeLabel(processed);
#else
    WGPUShaderModuleWGSLDescriptor shaderCodeDesc;
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    shaderCodeDesc.code = RenderUtils::MakeLabel(processed);
#endif
    WGPUShaderModuleDescriptor shaderDesc;
    shaderDesc.label = RenderUtils::MakeLabel("PPK");
    shaderDesc.nextInChain = &shaderCodeDesc.chain;

    if (RenderContext::IsReady())
    {
      m_ShaderModule = wgpuDeviceCreateShaderModule(RenderContext::GetDevice(), &shaderDesc);
    }
  }
}  // namespace WebEngine
