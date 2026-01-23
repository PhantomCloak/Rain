#include "GPUAllocator.h"
#include <cassert>
#include "core/Assert.h"
#include "render/Render.h"
#include "render/RenderContext.h"

namespace Rain
{
  int GPUAllocator::allocatedBufferCount = 0;
  int GPUAllocator::allocatedBufferTotalSize = 0;

  Ref<GPUBuffer> GPUAllocator::GAlloc(WGPUBufferUsageFlags usage, int size)
  {
    return GAlloc("", usage, size);
  }

  Ref<GPUBuffer> GPUAllocator::GAlloc(std::string label, WGPUBufferUsageFlags usage, int size)
  {
    // TODO check context
    WGPUBufferDescriptor bufferDesc = {};
    bufferDesc.size = size;
    bufferDesc.usage = usage;
    bufferDesc.mappedAtCreation = false;

    if (!label.empty())
    {
      // bufferDesc.label = label.c_str();
    }

    allocatedBufferCount++;
    allocatedBufferTotalSize += size;

    auto* RenderInstance = Render::Get();

    if (RenderInstance == nullptr || !RenderContext::IsReady())
    {
      return CreateRef<GPUBuffer>(nullptr, size);
    }

    return CreateRef<GPUBuffer>(wgpuDeviceCreateBuffer(RenderInstance->GetRenderContext()->GetDevice(), &bufferDesc), size);
  }

  void GPUBuffer::SetData(void const* data, int size)
  {
    SetData(data, 0, size);
  }

  void GPUBuffer::SetData(void const* data, int offset, int size)
  {
    if (auto* Instance = Render::Get())
    {
      if (auto renderContext = Instance->GetRenderContext())
      {
        RN_CORE_ASSERT(this->Buffer, "internal buffer of the GPUBuffer is not initialised.");
        wgpuQueueWriteBuffer(*renderContext->GetQueue(), this->Buffer, offset, data, size);
      }
    }
  }
}  // namespace Rain
