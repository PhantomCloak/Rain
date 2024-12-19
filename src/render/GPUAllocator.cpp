#include "GPUAllocator.h"
#include <cassert>
#include "core/Assert.h"
#include "render/Render.h"

Ref<RenderContext> GPUAllocator::m_Context;

int GPUAllocator::allocatedBufferCount = 0;
int GPUAllocator::allocatedBufferTotalSize = 0;

void GPUAllocator::Init() {
  m_Context = Render::Instance->GetRenderContext();
}

Ref<GPUBuffer> GPUAllocator::GAlloc(WGPUBufferUsage usage, int size) {
  return GAlloc("", usage, size);
}

Ref<GPUBuffer> GPUAllocator::GAlloc(std::string label, WGPUBufferUsage usage, int size) {
	// TODO check context
  WGPUBufferDescriptor bufferDesc = {};
  bufferDesc.size = size;
  bufferDesc.usage = usage;
  bufferDesc.mappedAtCreation = false;

  if (!label.empty()) {
    //bufferDesc.label = label.c_str();
  }

  allocatedBufferCount++;
  allocatedBufferTotalSize += size;

  return CreateRef<GPUBuffer>(wgpuDeviceCreateBuffer(m_Context->GetDevice(), &bufferDesc), size);
}

void GPUBuffer::SetData(void const* data, int size) {
  SetData(data, 0, size);
}

void GPUBuffer::SetData(void const* data, int offset, int size) {
  RN_CORE_ASSERT(this->Buffer, "internal buffer of the GPUBuffer is not initialised.");
  wgpuQueueWriteBuffer(*GPUAllocator::m_Context->GetQueue(), this->Buffer, offset, data, size);
}
