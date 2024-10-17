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

  if (stbi_is_hdr(pathStr.c_str())) {
    RN_LOG_ERR("Failed to import texture from file: {} - HDR Is not supported", pathStr);
  }

  int width, height, channels;
  imageBuffer.Data = stbi_load(pathStr.c_str(), &width, &height, &channels, 4 /* force RGBA */);
	//unsigned char* pixelData = stbi_load(pathStr.c_str(), &width, &height, &channels, 4 /* force RGBA */);
	//imageBuffer.Data = pixelData;
  imageBuffer.Size = width * height * 4;

	outFormat = TextureFormat::RGBA;

  outWidth = width;
  outHeight = height;

  return imageBuffer;
}
