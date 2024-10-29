#pragma once

#include <map>
#include "render/RenderUtils.h"
#include "render/Shader.h"
#include "webgpu/webgpu.h"
#include "render/Framebuffer.h"

enum PipelineCullingMode {
  BACK,
  FRONT,
  NONE
};

struct RenderPipelineSpec {
  VertexBufferLayout VertexLayout;
  VertexBufferLayout InstanceLayout;
  PipelineCullingMode CullingMode;
  Ref<Shader> Shader;
  Ref<Framebuffer> TargetFramebuffer;
  std::map<std::string, int> Overrides;  // We should get it from the shader but there is no translation lib atm

	std::string DebugName;
};

class RenderPipeline {
 public:
  RenderPipeline(const RenderPipelineSpec& props);
  RenderPipeline(std::string name, const RenderPipelineSpec& props);

  static Ref<RenderPipeline> Create(const RenderPipelineSpec& props){
    return CreateRef<RenderPipeline>(props);
  }

	const std::string& GetName() { return m_PipelineSpec.DebugName; }
	const RenderPipelineSpec& GetPipelineSpec() { return m_PipelineSpec; }
  const WGPURenderPipeline& GetPipeline() { return m_Pipeline; }

  void Invalidate();

  RenderPipelineSpec m_PipelineSpec;
  WGPURenderPipeline m_Pipeline;

 private:
};
