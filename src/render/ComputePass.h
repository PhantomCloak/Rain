//#pragma once
//#include "render/BindingManager.h"
//#include "render/GPUAllocator.h"
//#include "render/Pipeline.h"
//#include "render/Sampler.h"
//#include "render/Texture.h"
//
//
//struct RenderPassSpec {
//  Ref<RenderPipeline> Pipeline;
//  std::string DebugName;
//  glm::vec4 MarkerColor;
//};
//
//class ComputePass {
// public:
//  ComputePass(const ComputePassSpec& props);
//
//  void Set(const std::string& name, Ref<Texture> texture);
//  void Set(const std::string& name, Ref<GPUBuffer> uniform);
//  void Set(const std::string& name, Ref<Sampler> sampler);
//  void Bake();
//
//  const Ref<Texture> GetOutput(int index) { return GetTargetFrameBuffer()->GetAttachment(index); }
//  const Ref<Texture> GetDepthOutput() { return GetTargetFrameBuffer()->GetDepthAttachment(); }
//  const ComputePassSpec& GetProps() { return m_PassSpec; }
//  const Ref<BindingManager> GetBindManager() { return m_PassBinds; }
//	const Ref<Framebuffer> GetTargetFrameBuffer() { return m_PassSpec.Pipeline->GetPipelineSpec().TargetFramebuffer; }
//
//  static Ref<ComputePass> Create(const ComputePassSpec& spec);
//
//	void Prepare();
//
//  Ref<BindingManager> m_PassBinds;
//  RenderPassSpec m_PassSpec;
//};
