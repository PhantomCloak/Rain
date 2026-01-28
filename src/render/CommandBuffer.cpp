#include "CommandBuffer.h"
#include "core/Ref.h"
#include "render/RenderContext.h"
#include "render/RenderUtils.h"

namespace Rain
{
  void CommandBuffer::Begin()
  {
    WGPUCommandEncoderDescriptor commandEncoderDesc = {};
    ZERO_INIT(commandEncoderDesc);

    commandEncoderDesc.label = RenderUtils::MakeLabel("Default");
    commandEncoderDesc.nextInChain = nullptr;

    m_Encoder = wgpuDeviceCreateCommandEncoder(RenderContext::GetDevice(), &commandEncoderDesc);
  }

  void CommandBuffer::End()
  {
    WGPUCommandBufferDescriptor cmdBufferDescriptor;
    cmdBufferDescriptor.nextInChain = nullptr;
    cmdBufferDescriptor.label = RenderUtils::MakeLabel("CommandBuffer");

    m_Buffer = wgpuCommandEncoderFinish(m_Encoder, &cmdBufferDescriptor);
    wgpuCommandEncoderRelease(m_Encoder);
  }

  void CommandBuffer::Submit()
  {
    wgpuQueueSubmit(*RenderContext::GetQueue().get(), 1, &m_Buffer);

    wgpuCommandBufferRelease(m_Buffer);
  }
}  // namespace Rain
