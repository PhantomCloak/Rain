#pragma once
#include <cstdint>
#include "webgpu/webgpu.h"

namespace WebEngine
{
  class SwapChain
  {
   public:
    ~SwapChain();

    void Init(WGPUInstance instance, void* windowPtr);
    void Create(uint32_t width, uint32_t height);
    void Resize(uint32_t width, uint32_t height);

    void BeginFrame() {};
    void Present();

    WGPUTextureView GetSurfaceTextureView();
    WGPUSurface GetSurface() { return m_Surface; }

    uint32_t GetWidth() const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }

   private:
    WGPUSurface m_Surface = nullptr;
    WGPUTexture m_CurrentTexture = nullptr;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
  };
}  // namespace WebEngine
