#include "Render.h"

namespace Rain
{
  struct ShaderDependencies
  {
    std::vector<RenderPipeline*> Pipelines;
    std::vector<Material*> Materials;
  };

  Render* Render::m_RenderInstance = nullptr;

  static std::map<std::string, ShaderDependencies> s_ShaderDependencies;

  void Render::RegisterShaderDependency(Ref<Shader> shader, Material* material)
  {
    s_ShaderDependencies[shader->GetName()].Materials.push_back(material);
  }

  void Render::RegisterShaderDependency(Ref<Shader> shader, RenderPipeline* material)
  {
    s_ShaderDependencies[shader->GetName()].Pipelines.push_back(material);
  }

  void Render::ReloadShader(Ref<Shader> shader)
  {
    auto dependencies = s_ShaderDependencies[shader->GetName()];
    for (auto& material : dependencies.Materials)
    {
      material->OnShaderReload();
    }

    for (auto& pipeline : dependencies.Pipelines)
    {
      pipeline->Invalidate();
    }
  }
}  // namespace Rain
