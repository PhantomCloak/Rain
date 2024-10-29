#pragma once
#include "render/Texture.h"

struct FramebufferSpec {
  uint32_t Width = 0;
  uint32_t Height = 0;

  bool ClearColorOnLoad = true;
  bool ClearDepthOnLoad = true;
  bool SwapChainTarget = false;

  TextureFormat ColorFormat = TextureFormat::Undefined;
  TextureFormat DepthFormat = TextureFormat::Undefined;

  Ref<Texture> ExistingColorAttachment;
  Ref<Texture> ExistingDepth;

	std::vector<int> ExistingImageLayers;
  std::string DebugName;
};

class Framebuffer {
 public:
  Framebuffer(const FramebufferSpec& props);
  static Ref<Framebuffer> Create(const FramebufferSpec& props) {
    return CreateRef<Framebuffer>(props);
  }

	Ref<Texture> GetAttachment(int number = 0) { return m_ColorAttachment == nullptr ? m_FrameBufferSpec.ExistingColorAttachment : m_ColorAttachment; }
	Ref<Texture> GetDepthAttachment() { return m_DepthAttachment == nullptr ? m_FrameBufferSpec.ExistingDepth : m_DepthAttachment; }
	bool HasColorAttachment() { return m_FrameBufferSpec.SwapChainTarget || m_FrameBufferSpec.ExistingColorAttachment != nullptr || m_ColorAttachment != nullptr; }
	bool HasDepthAttachment() { return m_FrameBufferSpec.DepthFormat != TextureFormat::Undefined || m_FrameBufferSpec.ExistingDepth != nullptr; }
	void Resize(uint32_t width, uint32_t height);
  void Invalidate();

	const FramebufferSpec& GetFrameBufferSpec() { return m_FrameBufferSpec; }

	FramebufferSpec m_FrameBufferSpec;
 private:
  Ref<Texture> m_ColorAttachment;
  Ref<Texture> m_DepthAttachment;
};
