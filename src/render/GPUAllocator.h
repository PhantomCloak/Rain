#pragma once
#include <webgpu/webgpu.h>
#include <string>

struct GPUBuffer {
	WGPUBuffer Buffer;
	uint32_t Size;
};

class GPUAllocator {
 public:
  static void Init(WGPUDevice device);
  static GPUBuffer GAlloc(std::string label, WGPUBufferUsageFlags usage, int size);
  static GPUBuffer GAlloc(WGPUBufferUsageFlags usage, int size);

	static int allocatedBufferCount;
	static int allocatedBufferTotalSize;
 private:
	static WGPUDevice m_Device;
};
