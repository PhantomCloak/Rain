#pragma once

#include "render/Render.h"
#include "webgpu/webgpu.h"

namespace Rain
{
  class CommandEncoder
  {
  };

  class RenderPassEncoder
  {
  };

  class RenderPassEncoderWGPU : public RenderPassEncoder
  {
   public:
    RenderPassEncoderWGPU(WGPURenderPassEncoder passEncoder) : m_PassEncoder(passEncoder) {
                                                               };
    WGPURenderPassEncoder GetNativeEncoder() { return m_PassEncoder; }

   private:
    WGPURenderPassEncoder m_PassEncoder = nullptr;
  };

  class CommandEncoderWGPU : public CommandEncoder
  {
   public:
    CommandEncoderWGPU(WGPUCommandEncoder commandEncoder) : m_Encoder(commandEncoder) {};

    WGPUCommandEncoder GetNativeEncoder()
    {
      return m_Encoder;
    }

   private:
    WGPUCommandEncoder m_Encoder;
  };
}  // namespace Rain
