#pragma once
#include <webgpu/webgpu.h>
#include "glm/fwd.hpp"
#include "render/Texture.h"

class SwapChain {
 public:
  glm::vec2 GetDimensions();
  WGPUSwapChain GetNativeSwapChain() const { return m_SwapChain; }
	TextureFormat GetFormat();

 private:
  WGPUSwapChain m_SwapChain;
	uint32_t m_Width;
	uint32_t m_Height;
};
