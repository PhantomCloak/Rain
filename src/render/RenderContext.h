#pragma once

#include "core/Ref.h"
#include "webgpu/webgpu.h"

namespace Rain
{
  class RenderContext
  {
   public:
    static bool IsReady() { return m_Instance != nullptr; }
    static WGPUAdapter& GetAdapter() { return *Instance().m_Adapter; }
    static WGPUDevice& GetDevice() { return *Instance().m_Device; }
    static Ref<WGPUQueue> GetQueue() { return Instance().m_Queue; }
    static RenderContext& Instance() { return *m_Instance; };

    RenderContext() = default;
    RenderContext(WGPUAdapter adapter, WGPUDevice device, WGPUQueue queue)
        : m_Adapter(CreateRef<WGPUAdapter>(adapter)),
          m_Device(CreateRef<WGPUDevice>(device)),
          m_Queue(CreateRef<WGPUQueue>(queue)) { m_Instance = this; };

   private:
    static RenderContext* m_Instance;

    Ref<WGPUDevice> m_Device;
    Ref<WGPUQueue> m_Queue;
    Ref<WGPUSurface> m_Surface;
    Ref<WGPUAdapter> m_Adapter;

    friend class Render;
  };
}  // namespace Rain
