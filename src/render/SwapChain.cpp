#include "core/Assert.h"
#include "SwapChain.h"
#include "render/RenderContext.h"
#include "webgpu/webgpu.h"
#include "webgpu/webgpu_glfw.h"

namespace Rain
{
  void SwapChain::Init(WGPUInstance instance, void* windowPtr)
  {
    WGPUSurfaceDescriptor surfaceDescription{};
    const auto wnd = wgpu::glfw::SetupWindowAndGetSurfaceDescriptor(static_cast<GLFWwindow*>(windowPtr));
    surfaceDescription.nextInChain = reinterpret_cast<WGPUChainedStruct*>(wnd.get());

    m_Surface = wgpuInstanceCreateSurface(instance, &surfaceDescription);
  }

  void SwapChain::Create(uint32_t width, uint32_t height)
  {
    static WGPUTextureFormat m_swapChainFormat = WGPUTextureFormat_BGRA8Unorm;

    // WGPUSurfaceCapabilities* capabilities = new WGPUSurfaceCapabilities();
    // wgpuSurfaceGetCapabilities(m_Surface, m_Adapter, capabilities);

    WGPUSurfaceConfiguration config = {};
    config.device = RenderContext::GetDevice();
    config.format = m_swapChainFormat;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;
    config.width = width;
    config.height = height;
    config.presentMode = WGPUPresentMode_Fifo;
    config.viewFormatCount = 1;
    config.viewFormats = &m_swapChainFormat;
    wgpuSurfaceConfigure(m_Surface, &config);
  }

  void SwapChain::Present()
  {
    wgpuSurfacePresent(m_Surface);
  }

  WGPUTextureView SwapChain::GetSurfaceTextureView()
  {
    static WGPUTextureFormat m_swapChainFormat = WGPUTextureFormat_BGRA8Unorm;

    WGPUSurfaceTexture surfaceTexture = {};
    wgpuSurfaceGetCurrentTexture(m_Surface, &surfaceTexture);

#ifndef __EMSCRIPTEN__
    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal)
#else
    if (surfaceTexture.status != 0)
#endif
    {
      switch (surfaceTexture.status)
      {
        case WGPUSurfaceGetCurrentTextureStatus_Lost:
          // Reconfigure surface here
          // ConfigureSurface(m_swapChainDesc.width, m_swapChainDesc.height);
          wgpuSurfaceGetCurrentTexture(m_Surface, &surfaceTexture);
          break;
#ifndef __EMSCRIPTEN__
        case WGPUSurfaceGetCurrentTextureStatus_Error:
          RN_CORE_ASSERT(false, "Out of memory when acquiring next swapchain texture");
          return nullptr;
#else
        case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
          RN_CORE_ASSERT(false, "Out of memory when acquiring next swapchain texture");
          return nullptr;
#endif
        default:
          RN_CORE_ASSERT(false, "Unknown error when acquiring next swapchain texture");
          return nullptr;
      }
    }

    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.format = m_swapChainFormat;
    viewDesc.dimension = WGPUTextureViewDimension_2D;
    viewDesc.mipLevelCount = 1;
    viewDesc.arrayLayerCount = 1;
    viewDesc.aspect = WGPUTextureAspect_All;

    return wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);
  }
}  // namespace Rain
