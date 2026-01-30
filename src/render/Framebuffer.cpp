#include "Framebuffer.h"
#include "Application.h"
#include "render/Render.h"

namespace Rain
{
  Framebuffer::Framebuffer(const FramebufferSpec& props)
      : m_FrameBufferSpec(props)
  {
    if (m_FrameBufferSpec.Width == 0)
    {
      m_FrameBufferSpec.Width = Application::Get()->GetWindowSize().x;
      m_FrameBufferSpec.Height = Application::Get()->GetWindowSize().y;
    }

    Invalidate();
  }

  void Framebuffer::Resize(uint32_t width, uint32_t height)
  {
    m_FrameBufferSpec.Width = width;
    m_FrameBufferSpec.Height = height;
    Invalidate();
  }

  void Framebuffer::Invalidate()
  {
    if (m_DepthAttachment)
    {
      m_DepthAttachment->Resize(m_FrameBufferSpec.Width, m_FrameBufferSpec.Height);
    }
    else if (!m_FrameBufferSpec.ExistingDepth && m_FrameBufferSpec.DepthFormat != TextureFormat::Undefined)
    {
      TextureProps depthAttachmentSpec;
      depthAttachmentSpec.Format = m_FrameBufferSpec.DepthFormat;
      depthAttachmentSpec.Width = m_FrameBufferSpec.Width;
      depthAttachmentSpec.Height = m_FrameBufferSpec.Height;
      depthAttachmentSpec.CreateSampler = false;
      depthAttachmentSpec.MultiSample = m_FrameBufferSpec.Multisample;
      depthAttachmentSpec.DebugName = std::string("FBO_DA_") + m_FrameBufferSpec.DebugName;

      m_DepthAttachment = Texture2D::Create(depthAttachmentSpec);
    }

    if (m_FrameBufferSpec.SwapChainTarget && m_FrameBufferSpec.Multisample == 0)
    {
      return;
    }

    if (!m_ColorAttachment.empty())
    {
      for (const auto& attachment : m_ColorAttachment)
      {
        attachment->Resize(m_FrameBufferSpec.Width, m_FrameBufferSpec.Height);
      }
    }
    else if (!m_FrameBufferSpec.ExistingColorAttachment && m_FrameBufferSpec.ColorFormats.size() > 0)
    {
      for (const auto colorFormat : m_FrameBufferSpec.ColorFormats)
      {
        TextureProps colorAttachmentSpec;
        colorAttachmentSpec.Format = colorFormat;
        colorAttachmentSpec.Width = m_FrameBufferSpec.Width;
        colorAttachmentSpec.Height = m_FrameBufferSpec.Height;
        colorAttachmentSpec.CreateSampler = false;
        colorAttachmentSpec.MultiSample = m_FrameBufferSpec.Multisample;
        colorAttachmentSpec.DebugName = std::string("FBO_CA_") + m_FrameBufferSpec.DebugName;

        m_ColorAttachment.push_back(Texture2D::Create(colorAttachmentSpec));
      }
    }
  }
}  // namespace Rain
