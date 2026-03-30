#pragma once
#include <webgpu/webgpu.h>
#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <string>
#include "core/Buffer.h"
#include "core/UUID.h"
#include "render/Sampler.h"

namespace WebEngine
{
  enum class TextureFormat : uint8_t
  {
    RGBA8,
    BRGBA8,
    RGBA16F,
    RGBA32F,
    Depth24Plus,
    BC7,        // BC7 RGBA (desktop, requires TextureCompressionBC feature)
    ETC2_RGBA8, // ETC2 RGBA8 (mobile, requires TextureCompressionETC2 feature)
    ASTC_4x4,   // ASTC 4x4 RGBA (mobile/desktop, requires TextureCompressionASTC feature)
    Undefined
  };

  enum class TextureType : uint8_t
  {
    TextureDim2D,
    TextureDimCube
  };

  struct TextureProps
  {
    uint32_t Width = 1;
    uint32_t Height = 1;
    uint32_t MultiSample = 1;

    bool GenerateMips = false;
    bool CreateSampler = false;
    uint32_t Layers = 1;

    TextureFormat Format = TextureFormat::RGBA8;
    TextureWrappingFormat SamplerWrap = TextureWrappingFormat::Repeat;
    FilterMode SamplerFilter = FilterMode::Linear;

    std::string DebugName;
  };

  class Texture
  {
    virtual TextureFormat GetFormat() const = 0;

    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
    virtual uint32_t GetMipLevelCount() const = 0;

    virtual glm::uvec2 GetSize() const = 0;

   public:
    virtual TextureType GetType() const = 0;
    virtual WGPUTextureView GetReadView(int layer = 0) = 0;
    virtual WGPUTextureView GetWriteView(int layer = 0) = 0;
  };

  struct KTXImportResult;

  class Texture2D : public Texture
  {
   public:
    static Ref<Texture2D> Create(const TextureProps& props);
    static Ref<Texture2D> Create(const TextureProps& props, const std::filesystem::path& path);
    static Ref<Texture2D> CreateFromMemory(const TextureProps& props, Buffer imageData);
    static Ref<Texture2D> CreateFromKTX(const TextureProps& props, KTXImportResult& ktxData);

    void Resize(uint width, uint height);
    void Release();

    Texture2D();
    ~Texture2D();
    Texture2D(const TextureProps& props);
    Texture2D(const TextureProps& props, const std::filesystem::path& path);

    const int GetViewCount() { return (int)m_ReadViews.size(); }
    WGPUTextureView GetReadView(int layer = 0) override { return m_ReadViews[layer]; }
    WGPUTextureView GetWriteView(int layer = 0) override { return m_ReadViews[layer]; }

    uint32_t GetWidth() const override { return GetSize().x; }
    uint32_t GetHeight() const override { return GetSize().y; }
    uint32_t GetMipLevelCount() const override { return 15; }
    glm::uvec2 GetSize() const override { return glm::uvec2(m_TextureProps.Width, m_TextureProps.Height); }

    TextureFormat GetFormat() const override { return m_TextureProps.Format; }
    TextureType GetType() const override { return TextureType::TextureDim2D; }

    const TextureProps& GetSpec() { return m_TextureProps; }

    std::vector<WGPUTextureView> m_ReadViews;
    std::vector<WGPUTextureView> m_WriteViews;

    Ref<Sampler> Sampler;

   private:
    WGPUTexture TextureBuffer = NULL;
    TextureProps m_TextureProps;
    Buffer m_ImageData;

    void CreateFromFile(const TextureProps& props, const std::filesystem::path& path);
    void Invalidate();
  };

  class TextureCube : public Texture
  {
   public:
    static Ref<TextureCube> Create(const TextureProps& props);
    static Ref<TextureCube> Create(const TextureProps& props, const std::filesystem::path (&paths)[6]);
    TextureCube(const TextureProps& props, const std::filesystem::path (&paths)[6]);
    TextureCube(const TextureProps& props);
    TextureCube() {};
    ~TextureCube();

    WGPUTextureView GetReadView(int layer = 0) override { return m_ReadViews[layer]; }
    WGPUTextureView GetWriteView(int layer = 0) override { return m_WriteViews[layer]; }

    uint32_t GetWidth() const override { return GetSize().x; }
    uint32_t GetHeight() const override { return GetSize().y; }
    uint32_t GetMipLevelCount() const override { return 15; }
    glm::uvec2 GetSize() const override { return glm::uvec2(m_TextureProps.Width, m_TextureProps.Height); }

    TextureFormat GetFormat() const override { return m_TextureProps.Format; }
    TextureType GetType() const override { return TextureType::TextureDimCube; }
    const TextureProps& GetSpec() { return m_TextureProps; }

    std::vector<WGPUTextureView> m_ReadViews;
    std::vector<WGPUTextureView> m_WriteViews;

    WGPUTexture m_TextureBuffer = NULL;

   private:
    TextureProps m_TextureProps;
    Buffer m_ImageData[6];

    void CreateFromFile(const TextureProps& props, const std::filesystem::path (&paths)[6]);
    void Invalidate();
  };
}  // namespace WebEngine
