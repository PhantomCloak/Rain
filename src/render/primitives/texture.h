#pragma once
#include <webgpu/webgpu.h>
#include <string>

class Texture {
public:
	int id;
	std::string type;
	std::string path;
  WGPUTexture Texture;
  WGPUTextureView View;
};
