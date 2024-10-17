#pragma once
#include <webgpu/webgpu.h>
#include <glm/glm.hpp>
#include <string>
#include "core/UUID.h"
#include "render/Sampler.h"
#include "core/Buffer.h"
#include <filesystem>

enum TextureFormat {
  RGBA,
  BRGBA,
  Depth,
  Undefined
};

struct TextureProps {
 TextureFormat Format = TextureFormat::RGBA;
 uint32_t Width = 1; 
 uint32_t Height = 1; 
 TextureWrappingFormat SamplerWrap = TextureWrappingFormat::Repeat;
 FilterMode SamplerFilter = FilterMode::Linear;

 bool GenerateMips = true;
 bool CreateSampler = true;

 std::string DebugName;
};

class Texture {
 public:
  UUID Id;
  std::string type;
  std::string path;
  WGPUTexture TextureBuffer = NULL;
  WGPUTextureView View = NULL;
	Ref<Sampler> Sampler;

  static Ref<Texture> Create(const TextureProps& props);
  static Ref<Texture> Create(const TextureProps& props, const std::filesystem::path& path);

  void Resize(uint width, uint height);

	Texture();
	Texture(const TextureProps& props);
	Texture(const TextureProps& props, const std::filesystem::path& path);

 private:
	TextureProps m_TextureProps;
	Buffer m_ImageData;
	void CreateFromFile(const TextureProps& props, const std::filesystem::path& path);
  void Invalidate();
};
