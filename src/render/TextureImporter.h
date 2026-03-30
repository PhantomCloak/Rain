#pragma once
#include <filesystem>
#include <vector>
#include "core/Buffer.h"
#include "render/Texture.h"

namespace WebEngine {

  struct KTXMipLevel {
    uint64_t offset;
    uint64_t size;
    uint32_t width;
    uint32_t height;
  };

  struct KTXImportResult {
    Buffer data;                    // all mip data in one contiguous allocation
    std::vector<KTXMipLevel> mips;  // per-level metadata
    TextureFormat format = TextureFormat::Undefined;
    uint32_t baseWidth = 0;
    uint32_t baseHeight = 0;
    explicit operator bool() const { return data && !mips.empty(); }
  };

  class TextureImporter {
   public:
    static Buffer ImportFileToBuffer(const std::filesystem::path& path, TextureFormat& outFormat, uint32_t& outWidth, uint32_t& outHeight);
    static Buffer ImportFileToBufferExp(const std::filesystem::path& path, TextureFormat& outFormat, uint32_t& outWidth, uint32_t& outHeight);
    static KTXImportResult ImportKTXFromMemory(const uint8_t* data, size_t size);
  };
}  // namespace WebEngine
