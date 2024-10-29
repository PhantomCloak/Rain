#include "Framebuffer.h"
#include "Application.h"
#include "render/Render.h"

Framebuffer::Framebuffer(const FramebufferSpec& props)
    : m_FrameBufferSpec(props) {
  if (m_FrameBufferSpec.Width == 0) {
    m_FrameBufferSpec.Width = Application::Get()->GetWindowSize().x;
    m_FrameBufferSpec.Height = Application::Get()->GetWindowSize().y;
  }

  Invalidate();
}

void Framebuffer::Resize(uint32_t width, uint32_t height) {
  m_FrameBufferSpec.Width = width;
  m_FrameBufferSpec.Height = height;
  Invalidate();
}

void Framebuffer::Invalidate() {
  if (m_DepthAttachment) {
    m_DepthAttachment->Resize(m_FrameBufferSpec.Width, m_FrameBufferSpec.Height);
  } else if (!m_FrameBufferSpec.ExistingDepth && m_FrameBufferSpec.DepthFormat != TextureFormat::Undefined) {
    TextureProps depthAttachmentSpec;
    depthAttachmentSpec.Format = m_FrameBufferSpec.DepthFormat;
    depthAttachmentSpec.Width = m_FrameBufferSpec.Width;
    depthAttachmentSpec.Height = m_FrameBufferSpec.Height;
    depthAttachmentSpec.CreateSampler = false;
    depthAttachmentSpec.DebugName = std::string("FBO_DA_") + m_FrameBufferSpec.DebugName;

    m_DepthAttachment = Texture::Create(depthAttachmentSpec);
  }

  if (m_FrameBufferSpec.SwapChainTarget) {
    return;
  }

  if (m_ColorAttachment) {
    m_ColorAttachment->Resize(m_FrameBufferSpec.Width, m_FrameBufferSpec.Height);
  } else if (!m_FrameBufferSpec.ExistingColorAttachment && m_FrameBufferSpec.ColorFormat != TextureFormat::Undefined) {
    TextureProps colorAttachmentSpec;
    colorAttachmentSpec.Format = m_FrameBufferSpec.ColorFormat;
    colorAttachmentSpec.Width = m_FrameBufferSpec.Width;
    colorAttachmentSpec.Height = m_FrameBufferSpec.Height;
    colorAttachmentSpec.CreateSampler = false;
    colorAttachmentSpec.DebugName = std::string("FBO_CA_") + m_FrameBufferSpec.DebugName;

    m_ColorAttachment = Texture::Create(colorAttachmentSpec);
  }
}

//Ref<Texture> Framebuffer::GetDepthAttachment() {
//	if(m_DepthAttachment != nullptr) {
//		return m_DepthAttachment;
//	}
//
//	int layerNumber = m_FrameBufferSpec.ExistingImageLayers[0];
//	return m_FrameBufferSpec.ExistingDepth->m_Views[layerNumber];
//}
