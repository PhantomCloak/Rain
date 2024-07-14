#pragma once
#include "render/BindingManager.h"
#include "render/GPUAllocator.h"
#include "render/Pipeline.h"
#include "render/Sampler.h"
#include "render/Texture.h"

struct RenderPassProps {
  Ref<RenderPipeline> Pipeline;
  std::string DebugName;
  glm::vec4 MarkerColor;
};

// There is an interesting case in WebGPU which it doesn't have any explicit barriers and
// all synchronization goes implicitly within API, due to that all PASSES that are using the same resources
// Ran sequental instead of parallel, careness needed

class RenderPass {
 public:
  RenderPass(const RenderPassProps& props);

  void Set(const std::string& name, Ref<Texture> texture);
  void Set(const std::string& name, Ref<GPUBuffer> uniform);
  void Set(const std::string& name, Ref<Sampler> sampler);
  void Bake();

  const Ref<Texture> GetOutput(int index) { return m_PassProps.Pipeline->GetColorAttachment(); }
  const Ref<Texture> GetDepthOutput() { return m_PassProps.Pipeline->GetDepthAttachment(); }
  const RenderPassProps& GetProps() { return m_PassProps; }
  const Ref<BindingManager> GetBindManager() { return m_PassBinds; }

  static Ref<RenderPass> Create(const RenderPassProps& spec);

 private:
  Ref<BindingManager> m_PassBinds;
  RenderPassProps m_PassProps;
};
