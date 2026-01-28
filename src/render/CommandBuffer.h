#pragma once

#include "webgpu/webgpu.h"

namespace Rain
{
  class CommandBuffer
  {
   public:
    void Begin();
    void End();
    void Submit();

    WGPUCommandEncoder GetNativeEncoder() { return m_Encoder; }

   private:
    WGPUCommandEncoder m_Encoder;
    WGPUCommandBuffer m_Buffer;
  };
}  // namespace Rain
