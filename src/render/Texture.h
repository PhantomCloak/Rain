#pragma once
#include <webgpu/webgpu.h>
#include <string>
#include "core/UUID.h"
#include <glm/glm.hpp>

enum TextureFormat {
	RGBA,
	Depth,
	Undefined
};

struct TextureProps {
	glm::vec2 Dimensions;
	TextureFormat Format;
};

class Texture {
 public:
  UUID Id;
  std::string type;
  std::string path;
  WGPUTexture Buffer;
  WGPUTextureView View;

  static Ref<Texture> Create(TextureProps props);
};
