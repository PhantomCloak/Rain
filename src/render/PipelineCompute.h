//#pragma once
//#include <map>
//#include "render/Shader.h"
//#include "webgpu/webgpu.h"
//
//struct ComputePipelineSpec {
//  Ref<Shader> Shader;
//  std::map<std::string, int> Overrides;  // We should get it from the shader but there is no translation lib atm
//
//	std::string DebugName;
//};
//
//class PipelineCompute {
// public:
//  PipelineCompute(const ComputePipelineSpec& props);
//  PipelineCompute(std::string name, const ComputePipelineSpec& props);
//
//  static Ref<PipelineCompute> Create(const ComputePipelineSpec& props){
//    return CreateRef<PipelineCompute >(props);
//  }
//
//	const std::string& GetName() { return m_PipelineSpec.DebugName; }
//	const ComputePipelineSpec& GetPipelineSpec() { return m_PipelineSpec; }
//  const WGPUComputePipeline & GetPipeline() { return m_Pipeline; }
//
//  void Invalidate();
//
//  ComputePipelineSpec m_PipelineSpec;
//  WGPUComputePipeline m_Pipeline;
//};
