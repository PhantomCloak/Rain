#pragma once

#include "core/Ref.h"
#include "webgpu/webgpu.h"

class RenderContext {
 public:
  //static WGPUSwapChain& GetSwapChain() { return *Instance().m_SwapChain; }
  static WGPUSurface& GetSurface() { return *Instance().m_Surface; }
  static WGPUAdapter& GetAdapter() { return *Instance().m_Adapter; }
  static WGPUDevice& GetDevice() { return *Instance().m_Device; }
  static Ref<WGPUQueue> GetQueue() { return Instance().m_Queue; }
  static RenderContext& Instance() { return *m_Instance; };

  RenderContext() = default;
  RenderContext(WGPUAdapter adapter, WGPUSurface surface, WGPUDevice device, WGPUQueue queue)
      : m_Adapter(CreateRef<WGPUAdapter>(adapter)),
        m_Surface(CreateRef<WGPUSurface>(surface)),
        m_Device(CreateRef<WGPUDevice>(device)),
        m_Queue(CreateRef<WGPUQueue>(queue)) { m_Instance = this; };

 private:
  static RenderContext* m_Instance;

  Ref<WGPUDevice> m_Device;
  Ref<WGPUQueue> m_Queue;
  Ref<WGPUSurface> m_Surface;
  Ref<WGPUAdapter> m_Adapter;
  //Ref<WGPUSwapChain> m_SwapChain;

  friend class Render;
};
