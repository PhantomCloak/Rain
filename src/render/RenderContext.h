#pragma once

#include "core/Ref.h"
#include "webgpu/webgpu.h"

class RenderContext {
 public:
  static WGPUSwapChain& GetSwapChain() { return *Instance().m_SwapChain; }
  static WGPUDevice& GetDevice() { return *Instance().m_Device; }
  static Ref<WGPUQueue> GetQueue() { return Instance().m_Queue; }
  static RenderContext& Instance() { return *m_Instance; };

  RenderContext() = default;
  RenderContext(WGPUDevice device, WGPUQueue queue)
      : m_Device(CreateRef<WGPUDevice>(device)), m_Queue(CreateRef<WGPUQueue>(queue)) { m_Instance = this; };

 private:
  static RenderContext* m_Instance;

  Ref<WGPUDevice> m_Device;
  Ref<WGPUQueue> m_Queue;
  Ref<WGPUSwapChain> m_SwapChain;

  friend class Render;
};
