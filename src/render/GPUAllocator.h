#pragma once

#include <webgpu/webgpu.h>
#include <string>
#include "render/RenderContext.h"

class GPUBuffer;

class GPUAllocator {
 public:
  static void Init();
  static Ref<GPUBuffer> GAlloc(std::string label, WGPUBufferUsageFlags usage, int size);
  static Ref<GPUBuffer> GAlloc(WGPUBufferUsageFlags usage, int size);

  static int allocatedBufferCount;
  static int allocatedBufferTotalSize;

 private:
  static void GSet(Ref<GPUBuffer> buffer, void* data, int size);

 private:
  static Ref<RenderContext> m_Context;
  friend GPUBuffer;
};

class GPUBuffer {
 public:
  WGPUBuffer Buffer;
  int Size;
  GPUBuffer() {};
  GPUBuffer(WGPUBuffer buffer, int size)
      : Buffer(buffer), Size(size) {};

  void SetData(void const* data, int size);
  void SetData(void const* data, int offset, int size);

 private:
  friend GPUAllocator;
};
