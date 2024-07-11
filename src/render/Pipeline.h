#pragma once

#include <map>
#include "render/RenderUtils.h"
#include "render/Shader.h"
#include "render/Texture.h"
#include "webgpu/webgpu.h"

enum PipelineCullingMode {
  BACK,
  FRONT,
  NONE
};

struct RenderPipelineProps {
  VertexBufferLayout VertexLayout;
  VertexBufferLayout InstanceLayout;
  PipelineCullingMode CullingMode;
  Ref<Shader> VertexShader;
  Ref<Shader> FragmentShader;
  TextureFormat ColorFormat;
  TextureFormat DepthFormat;
	Ref<Texture> TargetFrameBuffer;
	Ref<Texture> TargetDepthBuffer;
  std::map<int, GroupLayout> groupLayout;  // We should get it from the shader but there is no translation lib atm
};

class RenderPipeline {
 public:
  RenderPipeline(std::string name, const RenderPipelineProps& props);

  static Ref<RenderPipeline> Create(std::string name, const RenderPipelineProps& props, Ref<Texture> colorAttachment = nullptr, Ref<Texture> depthAttachment = nullptr) {
    return CreateRef<RenderPipeline>(name, props);
  }

  const WGPURenderPipeline& GetPipeline() { return m_Pipeline; }
  const std::string& GetName() { return m_Name; }

	const Ref<Texture> GetColorAttachment() { return m_PipelineProps.TargetFrameBuffer; }
	const Ref<Texture> GetDepthAttachment() { return m_PipelineProps.TargetDepthBuffer; }
	const bool HasColorAttachment() { return m_PipelineProps.TargetFrameBuffer != nullptr; }
	const bool HasDepthAttachment() { return m_PipelineProps.TargetDepthBuffer != nullptr; }

  Ref<Shader> GetShader() const { return m_PipelineProps.FragmentShader; }

 private:
  std::string m_Name;
  RenderPipelineProps m_PipelineProps;
  WGPURenderPipeline m_Pipeline;
};
