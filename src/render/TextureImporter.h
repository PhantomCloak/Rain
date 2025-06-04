#pragma once
#include <filesystem>
#include "core/Buffer.h"
#include "render/Texture.h"

namespace Rain {
  class TextureImporter {
   public:
    static Buffer ImportFileToBuffer(const std::filesystem::path& path, TextureFormat& outFormat, uint32_t& outWidth, uint32_t& outHeight);
    static Buffer ImportFileToBufferExp(const std::filesystem::path& path, TextureFormat& outFormat, uint32_t& outWidth, uint32_t& outHeight);
  };
}  // namespace Rain
