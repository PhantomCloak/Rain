#pragma once

#include <map>
#include "render/RenderUtils.h"
#include "render/Texture.h"
#include "webgpu/webgpu.h"

enum PipelineCullingMode {
  BACK,
  FRONT,
  NONE
};

struct RenderPipelineInfo {
  Ref<Texture> m_DepthTexture;
  Ref<Texture> m_ColorAttachment;
};

struct RenderPipelineProps {
  PipelineCullingMode CullingMode;
  WGPUShaderModule VertexShader;
  WGPUShaderModule FragmentShader;
  TextureFormat ColorFormat;
  TextureFormat DepthFormat;
};

class RenderPipeline {
 public:
  RenderPipeline() = default;
  RenderPipeline(std::string name, RenderPipelineProps props, const std::vector<WGPUVertexBufferLayout>& vertexLayout, std::map<int, GroupLayout>& groupLayout);
  RenderPipelineInfo pipelineInfo;

  const WGPURenderPipeline& GetPipeline() { return m_Pipeline; }

 private:
  std::string m_Name;
	RenderPipelineProps m_PipelineProps;
  WGPURenderPipeline m_Pipeline;
};
