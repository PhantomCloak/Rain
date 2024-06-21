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
  std::map<int, GroupLayout> groupLayout;  // We should get it from the shader but there is no translation lib atm
};

class RenderPipeline {
 public:
  RenderPipeline(std::string name, const RenderPipelineProps& props, Ref<Texture> colorAttachment = nullptr, Ref<Texture> depthAttachment = nullptr);

  const WGPURenderPipeline& GetPipeline() { return m_Pipeline; }
  const std::string& GetName() { return m_Name; }

  const Ref<Texture> GetColorAttachment() { return m_ColorAttachment; }
  const Ref<Texture> GetDepthAttachment() { return m_DepthAttachment; }
  const bool HasColorAttachment() { return m_ColorAttachment != nullptr; }

  // TODO: We should apply proper free mechanism to avoid leaks in case of dereferring
  void SetColorAttachment(Ref<Texture> texture) { m_ColorAttachment = texture; }
  void SetDepthAttachment(Ref<Texture> texture) { m_DepthAttachment = texture; }

 private:
  std::string m_Name;
  Ref<Texture> m_DepthAttachment;
  Ref<Texture> m_ColorAttachment;
  RenderPipelineProps m_PipelineProps;
  WGPURenderPipeline m_Pipeline;
};
