#pragma once
#include <webgpu/webgpu.h>
#include <filesystem>
#include <glm/glm.hpp>
#include <string>
#include "core/Buffer.h"
#include "core/UUID.h"
#include "render/Sampler.h"

enum TextureFormat {
  RGBA8,
  BRGBA8,
  Depth24Plus,
  Undefined
};

struct TextureProps {
  TextureFormat Format = TextureFormat::RGBA8;
  uint32_t Width = 1;
  uint32_t Height = 1;
  TextureWrappingFormat SamplerWrap = TextureWrappingFormat::Repeat;
  FilterMode SamplerFilter = FilterMode::Linear;

  bool GenerateMips = false;
  bool CreateSampler = false;
  uint32_t layers = 1;

  std::string DebugName;
};

class Texture {
 public:
  UUID Id;
  std::string type;
  std::string path;
  WGPUTexture TextureBuffer = NULL;
  Ref<Sampler> Sampler;

  static Ref<Texture> Create(const TextureProps& props);
  static Ref<Texture> Create(const TextureProps& props, const std::filesystem::path& path);

  void Resize(uint width, uint height);
  void Release();

  Texture();
  Texture(const TextureProps& props);
  Texture(const TextureProps& props, const std::filesystem::path& path);

  WGPUTextureView GetNativeView(int layer = 0) { return m_Views[layer]; }

  std::vector<WGPUTextureView> m_Views;
	WGPUTextureView m_ArrayView;
 private:
  TextureProps m_TextureProps;
  Buffer m_ImageData;
  void CreateFromFile(const TextureProps& props, const std::filesystem::path& path);
  void Invalidate();
};
