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
  Undefined,
	ASTC6x6
};

enum TextureType {
  TextureDim2D,
  TextureDimCube
};

struct TextureProps {
  TextureFormat Format = TextureFormat::RGBA8;
  TextureWrappingFormat SamplerWrap = TextureWrappingFormat::Repeat;
  FilterMode SamplerFilter = FilterMode::Linear;

  uint32_t Width = 1;
  uint32_t Height = 1;
  uint32_t MultiSample = 1;

  bool GenerateMips = false;
  bool CreateSampler = false;
  uint32_t layers = 1;

  std::string DebugName;
};

class Texture {
  virtual TextureFormat GetFormat() const = 0;
  virtual TextureType GetType() const = 0;

  virtual uint32_t GetWidth() const = 0;
  virtual uint32_t GetHeight() const = 0;
  virtual uint32_t GetMipLevelCount() const = 0;

  virtual glm::uvec2 GetSize() const = 0;
};

class Texture2D : public Texture {
 public:
  UUID Id;
  std::string type;
  std::string path;
  WGPUTexture TextureBuffer = NULL;
  Ref<Sampler> Sampler;

  static Ref<Texture2D> Create(const TextureProps& props);
  static Ref<Texture2D> Create(const TextureProps& props, const std::filesystem::path& path);

  void Resize(uint width, uint height);
  void Release();

  Texture2D();
  Texture2D(const TextureProps& props);
  Texture2D(const TextureProps& props, const std::filesystem::path& path);

  const int GetViewCount() { return m_Views.size(); }
  WGPUTextureView GetNativeView(int layer = 0) { return m_Views[layer]; }
  const TextureProps& GetSpec() { return m_TextureProps; }

  glm::uvec2 GetSize() const override { return glm::uvec2(m_TextureProps.Width, m_TextureProps.Height); }

  uint32_t GetWidth() const override { return GetSize().x; }
  uint32_t GetHeight() const override { return GetSize().y; }
  uint32_t GetMipLevelCount() const override { return 15; }

  TextureFormat GetFormat() const override { return m_TextureProps.Format; }
  TextureType GetType() const override { return TextureType::TextureDim2D; }

  std::vector<WGPUTextureView> m_Views;
  WGPUTextureView m_ArrayView;

 private:
  TextureProps m_TextureProps;
  Buffer m_ImageData;
  void CreateFromFile(const TextureProps& props, const std::filesystem::path& path);
  void Invalidate();
};

// class TextureCube : public Texture {
//  public:
//   static Ref<TextureCube> Create(const TextureProps& props, const std::filesystem::path& path);
//   TextureCube(const TextureProps& props, const std::filesystem::path& path);
//
//   glm::uvec2 GetSize() const override { return glm::uvec2(m_TextureProps.Width, m_TextureProps.Height); }
//
//   uint32_t GetWidth() const override { return GetSize().x; }
//   uint32_t GetHeight() const override { return GetSize().y; }
//
//   TextureFormat GetFormat() const override { return m_TextureProps.Format; }
//   TextureType GetType() const override { return TextureType::TextureDimCube; }
//
//	void CreateFromFile(const TextureProps& props, const std::filesystem::path& path);
//   void Invalidate();
//
//  private:
//   TextureProps m_TextureProps;
//   Buffer m_ImageData;
// };
