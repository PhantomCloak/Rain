#pragma once
#include <cstdint>
#include "webgpu/webgpu.h"

namespace Rain
{
  class SwapChain
  {
   public:
    void Init(WGPUInstance instance, void* windowPtr);
    void Create(uint32_t width, uint32_t height);

    void BeginFrame() {};
    void Present();

    WGPUTextureView GetSurfaceTextureView();
    WGPUSurface GetSurface() { return m_Surface; }

   private:
    WGPUSurface m_Surface;
  };
}  // namespace Rain
