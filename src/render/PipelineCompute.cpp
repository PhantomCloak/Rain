//#include "render/PipelineCompute.h"
//#include "render/RenderContext.h"
//
//
//PipelineCompute::PipelineCompute (const ComputePipelineSpec& props)
//    : m_PipelineSpec(props) {
//  Invalidate();
//}
//
//void PipelineCompute ::Invalidate() {
//  WGPUComputePipelineDescriptor* pipelineDesc = (WGPUComputePipelineDescriptor*)malloc(sizeof(WGPUComputePipelineDescriptor));
//  pipelineDesc->label = m_PipelineSpec.DebugName.c_str();
//
//	// Create compute pipeline layout
//  std::vector<WGPUBindGroupLayout> bindGroupLayouts;
//  for (const auto& [_, layout] : m_PipelineSpec.Shader->GetReflectionInfo().LayoutDescriptors) {
//    bindGroupLayouts.push_back(layout);
//  }
//
//  WGPUPipelineLayoutDescriptor* layoutDesc = (WGPUPipelineLayoutDescriptor*)malloc(sizeof(WGPUPipelineLayoutDescriptor));
//  layoutDesc->bindGroupLayoutCount = bindGroupLayouts.size();
//  layoutDesc->bindGroupLayouts = bindGroupLayouts.data();
//  layoutDesc->nextInChain = nullptr;
//
//	auto device = RenderContext::GetDevice();
//	WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, layoutDesc);
//  pipelineDesc->layout = pipelineLayout;
//  pipelineDesc->nextInChain = nullptr;
//
//	// Create compute pipeline
//	WGPUComputePipelineDescriptor* computePipelineDesc = (WGPUComputePipelineDescriptor*)malloc(sizeof(WGPUComputePipelineDescriptor ));
//	computePipelineDesc->compute.constantCount = 0;
//	computePipelineDesc->compute.constants = nullptr;
//	computePipelineDesc->compute.entryPoint = "computeFilter";
//	computePipelineDesc->compute.module = m_PipelineSpec.Shader->GetNativeShaderModule();
//	computePipelineDesc->layout = pipelineLayout;
//
//	m_Pipeline = wgpuDeviceCreateComputePipeline(device, pipelineDesc);
//}
