#include "RenderPass.h"


RenderPass::RenderPass(const RenderPassProps& spec) : m_PassProps(spec)
{
	RN_ASSERT(spec.Pipeline != NULL, "RenderPass Pipeline {} cannot be null.", spec.DebugName);
  BindingSpec bindSpec;
  bindSpec.Name = spec.DebugName;
  bindSpec.ShaderRef = spec.Pipeline->GetShader();

	m_PassBinds = BindingManager::Create(bindSpec);
}

Ref<RenderPass> RenderPass::Create(const RenderPassProps& spec) {
	auto renderPass = CreateRef<RenderPass>(spec);
  return renderPass;
}

void RenderPass::Set(const std::string& name, Ref<Texture> texture) {
  m_PassBinds->Set(name, texture);
}
void RenderPass::Set(const std::string& name, Ref<GPUBuffer> uniform) {
  m_PassBinds->Set(name, uniform);
}
void RenderPass::Set(const std::string& name, Ref<Sampler> sampler) {
  m_PassBinds->Set(name, sampler);
}
void RenderPass::Bake() {
	m_PassBinds->Bake();
}

void RenderPass::Prepare() {
	m_PassBinds->InvalidateAndUpdate();
}
