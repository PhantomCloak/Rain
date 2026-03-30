#include "TextureImporter.h"
#include <cstring>
#include <ktx.h>
#include "render/RenderContext.h"

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

namespace WebEngine {
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

  KTXImportResult TextureImporter::ImportKTXFromMemory(const uint8_t* data, size_t size)
  {
    KTXImportResult result;

    ktxTexture2* ktxTex = nullptr;
    ktx_error_code_e err = ktxTexture2_CreateFromMemory(
        reinterpret_cast<const ktx_uint8_t*>(data),
        static_cast<ktx_size_t>(size),
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &ktxTex);

    if (err != KTX_SUCCESS || !ktxTex)
    {
      std::cerr << "KTX2 CreateFromMemory failed: " << ktxErrorString(err) << '\n';
      return result;
    }

    const char* ssName = "unknown";
    switch (ktxTex->supercompressionScheme) {
      case KTX_SS_NONE:           ssName = "None"; break;
      case KTX_SS_BASIS_LZ:      ssName = "BasisLZ (ETC1S)"; break;
      case KTX_SS_ZSTD:          ssName = "Zstd (UASTC)"; break;
      case KTX_SS_ZLIB:          ssName = "Zlib"; break;
      default: break;
    }
    std::cerr << "[KTX2] " << ktxTex->baseWidth << "x" << ktxTex->baseHeight
              << " mips=" << ktxTex->numLevels
              << " supercompression=" << ssName
              << " needsTranscode=" << ktxTexture2_NeedsTranscoding(ktxTex) << '\n';

    if (ktxTexture2_NeedsTranscoding(ktxTex))
    {
#ifdef USE_GPU_COMPRESSION
      auto& device = RenderContext::GetDevice();
      ktx_transcode_fmt_e ktxFmt = KTX_TTF_RGBA32;
      TextureFormat texFmt       = TextureFormat::RGBA8;

      if (wgpuDeviceHasFeature(device, WGPUFeatureName_TextureCompressionASTC))
      {
        ktxFmt = KTX_TTF_ASTC_4x4_RGBA;
        texFmt = TextureFormat::ASTC_4x4;
      }
      else if (wgpuDeviceHasFeature(device, WGPUFeatureName_TextureCompressionBC))
      {
        ktxFmt = KTX_TTF_BC7_RGBA;
        texFmt = TextureFormat::BC7;
      }
      else if (wgpuDeviceHasFeature(device, WGPUFeatureName_TextureCompressionETC2))
      {
        ktxFmt = KTX_TTF_ETC2_RGBA;
        texFmt = TextureFormat::ETC2_RGBA8;
      }

      err = ktxTexture2_TranscodeBasis(ktxTex, ktxFmt, 0);
      if (err != KTX_SUCCESS)
      {
        std::cerr << "KTX2 TranscodeBasis failed: " << ktxErrorString(err) << '\n';
        ktxTexture_Destroy(ktxTexture(ktxTex));
        return result;
      }
      result.format = texFmt;
#else
      err = ktxTexture2_TranscodeBasis(ktxTex, KTX_TTF_RGBA32, 0);
      if (err != KTX_SUCCESS)
      {
        std::cerr << "KTX2 TranscodeBasis failed: " << ktxErrorString(err) << '\n';
        ktxTexture_Destroy(ktxTexture(ktxTex));
        return result;
      }
      result.format = TextureFormat::RGBA8;
#endif
    }
    else
    {
      result.format = TextureFormat::RGBA8;
    }

    result.baseWidth  = ktxTex->baseWidth;
    result.baseHeight = ktxTex->baseHeight;

    uint32_t numLevels = ktxTex->numLevels;

    size_t totalSize = 0;
    for (uint32_t level = 0; level < numLevels; level++)
    {
      totalSize += ktxTexture_GetImageSize(ktxTexture(ktxTex), level);
    }

    result.data.Size = totalSize;
    result.data.Data = new byte[totalSize];

    size_t currentOffset = 0;
    uint32_t mipWidth  = result.baseWidth;
    uint32_t mipHeight = result.baseHeight;

    for (uint32_t level = 0; level < numLevels; level++)
    {
      ktx_size_t imageOffset = 0;
      ktxTexture_GetImageOffset(ktxTexture(ktxTex), level, 0, 0, &imageOffset);
      ktx_size_t imageSize = ktxTexture_GetImageSize(ktxTexture(ktxTex), level);

      memcpy(static_cast<uint8_t*>(result.data.Data) + currentOffset,
             ktxTex->pData + imageOffset,
             imageSize);

      result.mips.push_back({currentOffset, imageSize, mipWidth, mipHeight});

      currentOffset += imageSize;
      mipWidth  = std::max(1u, mipWidth / 2);
      mipHeight = std::max(1u, mipHeight / 2);
    }

    ktxTexture_Destroy(ktxTexture(ktxTex));
    return result;
  }
}  // namespace WebEngine
