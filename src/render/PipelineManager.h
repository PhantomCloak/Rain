#pragma once
#include <webgpu/webgpu.h>
#include <map>
#include <string>
#include "render/RenderUtils.h"
#include "render/ShaderManager.h"

class PipelineManager {
 public:
  PipelineManager(WGPUDevice& device, std::shared_ptr<ShaderManager> shaderManager)
      : _device(device), shaderManager_(shaderManager){};

  WGPURenderPipeline CreatePipeline(const std::string& pipelineId,
			const std::string& shaderId,
			WGPUVertexBufferLayout vertexLayout,
			GroupLayout groupLayout,
			WGPUTextureFormat depthFormat,
			WGPUTextureFormat colorFormat,
			WGPUCullMode cullingMode,
			WGPUSurface surface,
			WGPUAdapter adapter);
  // Get a pipeline by ID
  WGPURenderPipeline GetPipeline(const std::string& pipelineId);

 private:
  WGPUDevice& _device;
  std::shared_ptr<ShaderManager> shaderManager_;
  std::map<std::string, WGPURenderPipeline> pipelines_;
};
