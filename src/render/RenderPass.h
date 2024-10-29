#pragma once
#include "render/BindingManager.h"
#include "render/GPUAllocator.h"
#include "render/Pipeline.h"
#include "render/Sampler.h"
#include "render/Texture.h"

struct RenderPassSpec {
  Ref<RenderPipeline> Pipeline;
  std::string DebugName;
  glm::vec4 MarkerColor;
};

// There is an interesting case in WebGPU which it doesn't have any explicit barriers and
// all synchronization goes implicitly within API, due to that all PASSES that are using the same resources
// Ran sequental instead of parallel, careness needed

class RenderPass {
 public:
  RenderPass(const RenderPassSpec& props);

  void Set(const std::string& name, Ref<Texture> texture);
  void Set(const std::string& name, Ref<GPUBuffer> uniform);
  void Set(const std::string& name, Ref<Sampler> sampler);
  void Bake();

  const Ref<Texture> GetOutput(int index) { return GetTargetFrameBuffer()->GetAttachment(index); }
  const Ref<Texture> GetDepthOutput() { return GetTargetFrameBuffer()->GetDepthAttachment(); }
  const RenderPassSpec& GetProps() { return m_PassSpec; }
  const Ref<BindingManager> GetBindManager() { return m_PassBinds; }
	const Ref<Framebuffer> GetTargetFrameBuffer() { return m_PassSpec.Pipeline->GetPipelineSpec().TargetFramebuffer; }

  static Ref<RenderPass> Create(const RenderPassSpec& spec);

	void Prepare();

  Ref<BindingManager> m_PassBinds;
  RenderPassSpec m_PassSpec;
};
