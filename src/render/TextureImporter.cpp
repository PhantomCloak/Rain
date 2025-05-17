#include "TextureImporter.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#endif
#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#endif

#include <stb_image.h>

Buffer TextureImporter::ImportFileToBuffer(const std::filesystem::path& path, TextureFormat& outFormat, uint32_t& outWidth, uint32_t& outHeight) {
  Buffer imageBuffer;
  std::string pathStr = path.string();

  int width, height, channels;

  if (stbi_is_hdr(pathStr.c_str())) {
    imageBuffer.Data = (byte*)stbi_loadf(pathStr.c_str(), &width, &height, &channels, 4);
    imageBuffer.Size = width * height * 4 * sizeof(float);
    outFormat = TextureFormat::RGBA32F;
  } else {
    imageBuffer.Data = stbi_load(pathStr.c_str(), &width, &height, &channels, 4 /* force RGBA */);
    imageBuffer.Size = width * height * 4;
    outFormat = TextureFormat::RGBA8;
  }

  outWidth = width;
  outHeight = height;

  return imageBuffer;
}
