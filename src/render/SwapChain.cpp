#include "core/Assert.h"
#include "SwapChain.h"
#include "render/RenderContext.h"
#include "webgpu/webgpu.h"

#ifndef __EMSCRIPTEN__
#include "webgpu/webgpu_glfw.h"
#endif

namespace WebEngine
{
  SwapChain::~SwapChain()
  {
    if (m_CurrentTexture)
    {
      wgpuTextureRelease(m_CurrentTexture);
      m_CurrentTexture = nullptr;
    }
    if (m_Surface)
    {
      wgpuSurfaceUnconfigure(m_Surface);
      wgpuSurfaceRelease(m_Surface);
      m_Surface = nullptr;
    }
  }

  void SwapChain::Init(WGPUInstance instance, void* windowPtr)
  {
#ifndef __EMSCRIPTEN__
    WGPUSurfaceDescriptor surfaceDescription{};
    const auto wnd = wgpu::glfw::SetupWindowAndGetSurfaceDescriptor(static_cast<GLFWwindow*>(windowPtr));
    surfaceDescription.nextInChain = reinterpret_cast<WGPUChainedStruct*>(wnd.get());
    m_Surface = wgpuInstanceCreateSurface(instance, &surfaceDescription);
    RN_LOG("SwapChain::Init - Surface created: {}", m_Surface != nullptr);
#else
    WGPUSurfaceDescriptorFromCanvasHTMLSelector* canvasDesc = ZERO_ALLOC(WGPUSurfaceDescriptorFromCanvasHTMLSelector);
    canvasDesc->chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
    canvasDesc->selector = "#canvas";

    WGPUSurfaceDescriptor* surfaceDesc = ZERO_ALLOC(WGPUSurfaceDescriptor);
    surfaceDesc->nextInChain = &canvasDesc->chain;
    m_Surface = wgpuInstanceCreateSurface(instance, surfaceDesc);
#endif
  }

  void SwapChain::Create(uint32_t width, uint32_t height)
  {
    static WGPUTextureFormat m_swapChainFormat = WGPUTextureFormat_BGRA8Unorm;

    m_Width = width;
    m_Height = height;

    RN_LOG("SwapChain::Create - Configuring surface {}x{}", width, height);

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

  void SwapChain::Resize(uint32_t width, uint32_t height)
  {
    if (width == 0 || height == 0)
      return;
    if (width == m_Width && height == m_Height)
      return;
    Create(width, height);
  }

  void SwapChain::Present()
  {
#ifndef __EMSCRIPTEN__
    if (m_CurrentTexture)
    {
      wgpuSurfacePresent(m_Surface);
    }
#endif
  }

  WGPUTextureView SwapChain::GetSurfaceTextureView()
  {
    static WGPUTextureFormat swapChainFormat = WGPUTextureFormat_BGRA8Unorm;

    if (m_CurrentTexture)
    {
      wgpuTextureRelease(m_CurrentTexture);
      m_CurrentTexture = nullptr;
    }

    WGPUSurfaceTexture surfaceTexture = {};
    wgpuSurfaceGetCurrentTexture(m_Surface, &surfaceTexture);

#ifndef __EMSCRIPTEN__
    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal
        && surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
#else
    if (surfaceTexture.status != 0)
#endif
    {
      switch (surfaceTexture.status)
      {
        case WGPUSurfaceGetCurrentTextureStatus_Lost:
        case WGPUSurfaceGetCurrentTextureStatus_Outdated:
          if (m_Width > 0 && m_Height > 0)
            Create(m_Width, m_Height);
          return nullptr;
#ifndef __EMSCRIPTEN__
        case WGPUSurfaceGetCurrentTextureStatus_Error:
          RN_CORE_ASSERT(false, "Error when acquiring next swapchain texture");
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

    m_CurrentTexture = surfaceTexture.texture;

    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.format = swapChainFormat;
    viewDesc.dimension = WGPUTextureViewDimension_2D;
    viewDesc.mipLevelCount = 1;
    viewDesc.arrayLayerCount = 1;
    viewDesc.aspect = WGPUTextureAspect_All;

    return wgpuTextureCreateView(m_CurrentTexture, &viewDesc);
  }
}  // namespace WebEngine
