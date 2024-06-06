#include "GPUAllocator.h"
#include <cassert>
#include "core/Assert.h"

WGPUDevice GPUAllocator::m_Device;

int GPUAllocator::allocatedBufferCount = 0;
int GPUAllocator::allocatedBufferTotalSize = 0;

void GPUAllocator::Init(WGPUDevice device) {
	m_Device = device;
}


GPUBuffer GPUAllocator::GAlloc(WGPUBufferUsageFlags usage, int size) {
	return GAlloc("", usage, size);
}

GPUBuffer GPUAllocator::GAlloc(std::string label, WGPUBufferUsageFlags usage, int size) {
	RN_ASSERT(m_Device != NULL, "GPUAllocator::GAlloc: Device is not initialized.");


  WGPUBufferDescriptor bufferDesc = {};
  bufferDesc.size = size;
  bufferDesc.usage = usage;
  bufferDesc.mappedAtCreation = false;


	if(!label.empty()) {
		bufferDesc.label = label.c_str();
	}

	allocatedBufferCount++;
  allocatedBufferTotalSize += size;

  return GPUBuffer {
		.Buffer = wgpuDeviceCreateBuffer(m_Device, &bufferDesc),
		.Size = (uint32_t)size
	};
}
