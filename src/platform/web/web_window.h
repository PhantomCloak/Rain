#pragma once
#include <string>
#include <webgpu/webgpu.h>

extern "C" WGPUSurface htmlGetCanvasSurface(WGPUInstance instance, std::string selector) {
  WGPUSurfaceDescriptorFromCanvasHTMLSelector canvasDescriptor = {
      .chain =
          (WGPUChainedStruct){
              .next = NULL,
              .sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector,
          },
      .selector = selector.c_str()};

  WGPUSurfaceDescriptor surfaceDescriptor = {
      .label = NULL,
      .nextInChain = (const WGPUChainedStruct *)&canvasDescriptor};

  WGPUSurface surface = wgpuInstanceCreateSurface(instance, &surfaceDescriptor);

  return surface;
}
