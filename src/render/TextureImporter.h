#pragma once
#include "core/Buffer.h"
#include <filesystem>
#include "render/Texture.h"

class TextureImporter
{
 public:
	 static Buffer ImportFileToBuffer(const std::filesystem::path& path, TextureFormat& outFormat, uint32_t& outWidth, uint32_t& outHeight);
	 static Buffer ImportFileToBufferExp(const std::filesystem::path& path, TextureFormat& outFormat, uint32_t& outWidth, uint32_t& outHeight);
};
