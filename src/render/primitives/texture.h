#pragma once
#include <webgpu/webgpu.h>

class Texture {
public:
  WGPUTexture Texture;
  WGPUTextureView View;
};
