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
  RGBA16F,
  RGBA32F,
  Depth24Plus,
  Undefined
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

 public:
  virtual bool Hack() const = 0;
  virtual WGPUTextureView GetView() = 0;
  virtual WGPUTextureView GetReadableView(int layer = 0) = 0;
  virtual WGPUTextureView GetWriteableView(int layer = 0) = 0;
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

  const int GetViewCount() { return m_ReadViews.size(); }
  WGPUTextureView GetView() override { return m_View; }
  WGPUTextureView GetReadableView(int layer = 0) override { return m_ReadViews[layer]; }
  WGPUTextureView GetWriteableView(int layer = 0) override { return m_ReadViews[layer]; }

  glm::uvec2 GetSize() const override { return glm::uvec2(m_TextureProps.Width, m_TextureProps.Height); }

  uint32_t GetWidth() const override { return GetSize().x; }
  uint32_t GetHeight() const override { return GetSize().y; }
  uint32_t GetMipLevelCount() const override { return 15; }
  bool Hack() const override { return false; }

  TextureFormat GetFormat() const override { return m_TextureProps.Format; }
  TextureType GetType() const override { return TextureType::TextureDim2D; }

  const TextureProps& GetSpec() { return m_TextureProps; }

  WGPUTextureView m_View;;
  std::vector<WGPUTextureView> m_ReadViews;
  std::vector<WGPUTextureView> m_WriteViews;

  WGPUTextureView m_ArrayView;

 private:
  TextureProps m_TextureProps;
  Buffer m_ImageData;

  void CreateFromFile(const TextureProps& props, const std::filesystem::path& path);
  void Invalidate();
};

class TextureCube : public Texture {
 public:
  static Ref<TextureCube> Create(const TextureProps& props);
  static Ref<TextureCube> Create(const TextureProps& props, const std::filesystem::path (&paths)[6]);
  TextureCube(const TextureProps& props, const std::filesystem::path (&paths)[6]);
  TextureCube(const TextureProps& props);
  TextureCube() {};

  glm::uvec2 GetSize() const override { return glm::uvec2(m_TextureProps.Width, m_TextureProps.Height); }

  uint32_t GetWidth() const override { return GetSize().x; }
  uint32_t GetHeight() const override { return GetSize().y; }
  uint32_t GetMipLevelCount() const override { return 15; }

  TextureFormat GetFormat() const override { return m_TextureProps.Format; }
  TextureType GetType() const override { return TextureType::TextureDimCube; }

  const TextureProps& GetSpec() { return m_TextureProps; }
  bool Hack() const override { return true; }

  WGPUTextureView GetView() override { return m_View; }
  WGPUTextureView GetReadableView(int layer = 0) override { return m_ReadViews[layer]; }
  WGPUTextureView GetWriteableView(int layer = 0) override { return m_WriteViews[layer]; }

  WGPUTexture m_TextureBuffer = NULL;

  WGPUTextureView m_View;;
  std::vector<WGPUTextureView> m_ReadViews;
  std::vector<WGPUTextureView> m_WriteViews;

 private:
  TextureProps m_TextureProps;
  Buffer m_ImageData[6];

  void CreateFromFile(const TextureProps& props, const std::filesystem::path (&paths)[6]);
  void Invalidate();
};
