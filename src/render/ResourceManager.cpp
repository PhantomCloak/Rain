#include "ResourceManager.h"
#include <stb_image.h>
#include <stb_image_resize2.h>
#include <iostream>
#include <vector>
#include "core/Assert.h"
#include "core/Log.h"
#include "debug/Profiler.h"
#include "render/RenderContext.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#ifndef STB_IMAGE_RESIZE2_IMPLEMENTATION
#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#endif
#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#endif

#include "render/Render.h"

namespace Rain {
  std::unordered_map<std::string, std::shared_ptr<Texture2D>> Rain::ResourceManager::_loadedTextures;
  std::unordered_map<std::string, std::shared_ptr<TextureCube>> Rain::ResourceManager::_loadedTexturesCube;

  std::unordered_map<AssetHandle, Ref<MeshSource>> Rain::ResourceManager::m_LoadedMeshSources;

  void WriteTexture2(void* pixelData, const WGPUTexture& target, uint32_t width, uint32_t height, uint32_t targetMip) {
    Ref<RenderContext> renderContext = Render::Instance->GetRenderContext();
    auto queue = renderContext->GetQueue();
    WGPUOrigin3D targetOrigin = {0, 0, 0};

    WGPUImageCopyTexture dest = {
        .texture = target,
        .mipLevel = targetMip,
        .origin = targetOrigin,
        .aspect = WGPUTextureAspect_All};

    WGPUTextureDataLayout textureLayout = {
        .offset = 0,
        .bytesPerRow = 4 * width,
        .rowsPerImage = height};

    WGPUExtent3D textureSize = {.width = width, .height = height, .depthOrArrayLayers = 1};
    wgpuQueueWriteTexture(*queue, &dest, pixelData, (4 * width * height), &textureLayout, &textureSize);
  }

  void writeMipMaps(
      WGPUDevice m_device,
      WGPUTexture m_texture,
      WGPUExtent3D textureSize,
      uint32_t mipLevelCount,
      const unsigned char* pixelData,
      WGPUOrigin3D targetOrigin = {0, 0, 0}) {
    RN_PROFILE_FUNCN("Generate Mips");
    auto m_queue = wgpuDeviceGetQueue(m_device);

    WGPUImageCopyTexture destination = {
        .texture = m_texture,
        .mipLevel = 0,
        .origin = targetOrigin,
        .aspect = WGPUTextureAspect_All};

    WGPUTextureDataLayout currentTextureLayout = {
        .offset = 0,
        .bytesPerRow = 4 * textureSize.width,
        .rowsPerImage = textureSize.height};

    auto origSize = (4 * textureSize.width * textureSize.height);
    wgpuQueueWriteTexture(m_queue, &destination, pixelData, origSize, &currentTextureLayout, &textureSize);
    // WriteTexture2((void*)pixelData, m_texture, (uint32_t)textureSize.width, (uint32_t)textureSize.height, 0);

    std::vector<unsigned char> prevPixelBuffer(origSize);
    std::memcpy(prevPixelBuffer.data(), pixelData, origSize);

    WGPUExtent3D currentWriteInfo = textureSize;
    int prevWidth = textureSize.width, prevHeight = textureSize.height;

    for (uint32_t level = 1; level < mipLevelCount; level++) {
      currentWriteInfo.width = currentWriteInfo.width > 1 ? currentWriteInfo.width / 2 : 1;
      currentWriteInfo.height = currentWriteInfo.height > 1 ? currentWriteInfo.height / 2 : 1;
      std::vector<unsigned char> buffer(4 * currentWriteInfo.width * currentWriteInfo.height);

      stbir_resize_uint8_linear(prevPixelBuffer.data(),
                                prevWidth,
                                prevHeight,
                                0,
                                buffer.data(),
                                currentWriteInfo.width,
                                currentWriteInfo.height,
                                0,
                                STBIR_RGBA);

      currentTextureLayout.bytesPerRow = 4 * currentWriteInfo.width;
      currentTextureLayout.rowsPerImage = currentWriteInfo.height;
      destination.mipLevel = level;

      wgpuQueueWriteTexture(m_queue, &destination, buffer.data(), buffer.size(), &currentTextureLayout, &currentWriteInfo);
      // WriteTexture2((void*)buffer.data(), m_texture, currentWriteInfo.width, currentWriteInfo.height, level);

      prevWidth = currentWriteInfo.width;
      prevHeight = currentWriteInfo.height;

      prevPixelBuffer = std::move(buffer);
    }

    wgpuQueueRelease(m_queue);
  }

  WGPUTexture loadTexture(const char* path, WGPUDevice m_device, WGPUTextureView* pTextureView) {
    int width, height, channels;
    unsigned char* pixelData = stbi_load(path, &width, &height, &channels, 4 /* force RGBA */);
    if (pixelData == NULL) {
      return NULL;
    }

    // Create Texture
    WGPUTextureDescriptor textureDesc = {};
    textureDesc.dimension = WGPUTextureDimension_2D;
    textureDesc.format = WGPUTextureFormat_RGBA8Unorm;
    textureDesc.size.width = (uint32_t)width;
    textureDesc.size.height = (uint32_t)height;
    textureDesc.size.depthOrArrayLayers = 1;
    textureDesc.mipLevelCount = (uint32_t)(floor((float)(log2(glm::max(width, height))))) + 1;  // can be replaced with bit_width in C++ 20
    textureDesc.sampleCount = 1;
    textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;

    WGPUTexture m_texture = wgpuDeviceCreateTexture(m_device, &textureDesc);

    writeMipMaps(m_device, m_texture, textureDesc.size, textureDesc.mipLevelCount, pixelData);

    // Free image data
    stbi_image_free(pixelData);

    // Create Texture View if requested
    if (pTextureView != NULL) {
      WGPUTextureViewDescriptor textureViewDesc = {};
      textureViewDesc.aspect = WGPUTextureAspect_All;
      textureViewDesc.baseArrayLayer = 0;
      textureViewDesc.arrayLayerCount = 1;
      textureViewDesc.baseMipLevel = 0;
      textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
      textureViewDesc.dimension = WGPUTextureViewDimension_2D;
      textureViewDesc.format = textureDesc.format;

      *pTextureView = wgpuTextureCreateView(m_texture, &textureViewDesc);
    }

    return m_texture;
  }

  bool Rain::ResourceManager::IsTextureExist(std::string id) {
    return _loadedTextures.find(id) != _loadedTextures.end();
  }

  // TOOD: check if file exist or not
  std::shared_ptr<Texture2D> Rain::ResourceManager::LoadTexture(std::string id, std::string path) {
    RN_PROFILE_FUNC;

    TextureProps textureProp = {};
    textureProp.DebugName = id;
    textureProp.CreateSampler = true;
    textureProp.GenerateMips = true;

    auto p = std::filesystem::path(path);
    auto texture = Texture2D::Create(textureProp, p);

    _loadedTextures[id] = texture;

    RN_LOG("Texture {} loaded from {}", id, path);
    return texture;
  }

  std::shared_ptr<Texture2D> Rain::ResourceManager::GetTexture(std::string id) {
    if (_loadedTextures.find(id) == _loadedTextures.end()) {
      std::cout << "GetTexture for id " << id << " does not exist" << std::endl;
      return nullptr;
    }

    std::shared_ptr<Texture2D>& texture = _loadedTextures[id];

    if (!texture) {
      std::cout << "Texture is invalid" << std::endl;
    }

    return texture;
  }

  Ref<MeshSource> Rain::ResourceManager::GetMeshSource(UUID handle) {
    RN_ASSERT(m_LoadedMeshSources.find(handle) != m_LoadedMeshSources.end());
    return m_LoadedMeshSources[handle];
  }

  Ref<MeshSource> Rain::ResourceManager::LoadMeshSource(std::string path) {
    Ref<MeshSource> meshSource = CreateRef<MeshSource>(path);
    m_LoadedMeshSources[meshSource->Id] = meshSource;
    return meshSource;
  }

  std::shared_ptr<TextureCube> Rain::ResourceManager::LoadCubeTexture(std::string id, const std::filesystem::path (&paths)[6]) {
    TextureProps textureProp = {};
    textureProp.DebugName = id;
    textureProp.CreateSampler = true;
    textureProp.GenerateMips = true;

    auto texture = TextureCube::Create(textureProp, paths);

    _loadedTexturesCube[id] = texture;

    return texture;
  }

  // std::shared_ptr<TextureCube> Rain::ResourceManager::LoadCubeTexture(std::string id, const std::filesystem::path (&paths)[6]) {
  //   WGPUExtent3D cubemapSize = {0, 0, 6};
  //   std::array<uint8_t*, 6> pixelData;
  //
  //   for (uint32_t layer = 0; layer < 6; ++layer) {
  //     int width, height, channels;
  //     pixelData[layer] = stbi_load(paths[layer].c_str(), &width, &height, &channels, 4);
  //
  //     //RN_ASSERT(pixelData[layer] != nullptr, "Cubemap texture couldn't found at {}!", paths[layer]);
  //
  //     if (layer == 0) {
  //       cubemapSize.width = (uint32_t)width;
  //       cubemapSize.height = (uint32_t)height;
  //     } else {
  //      // RN_ASSERT(cubemapSize.width == (uint32_t)width && cubemapSize.height == (uint32_t)height, "All cubemap texture faces should be in the same size.", paths[layer]);
  //     }
  //   }
  //
  //   WGPUTextureDescriptor textureDesc;
  //   textureDesc.label = "bb";
  //   textureDesc.dimension = WGPUTextureDimension_2D;
  //   //textureDesc.format = WGPUTextureFormat_RGBA8Unorm;
  //   textureDesc.format = WGPUTextureFormat_RGBA8Unorm;
  //   textureDesc.size = cubemapSize;
  //   textureDesc.mipLevelCount = (uint32_t)(floor((float)(log2(glm::max(textureDesc.size.width, textureDesc.size.height))))) + 1;  // can be replaced with bit_width in C++ 20
  //   textureDesc.sampleCount = 1;
  //   textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
  //   textureDesc.viewFormatCount = 0;
  //   textureDesc.viewFormats = nullptr;
  //   textureDesc.nextInChain = nullptr;
  //   WGPUTexture texture = wgpuDeviceCreateTexture(RenderContext::GetDevice(), &textureDesc);
  //
  //   WGPUExtent3D cubemapLayerSize = {cubemapSize.width, cubemapSize.height, 1};
  //   for (uint32_t layer = 0; layer < 6; ++layer) {
  //     WGPUOrigin3D origin = {0, 0, layer};
  //     writeMipMaps(RenderContext::GetDevice(), texture, cubemapLayerSize, textureDesc.mipLevelCount, pixelData[layer], origin);
  //     stbi_image_free(pixelData[layer]);
  //   }
  //
  //   auto tex = std::make_shared<TextureCube>();
  //   tex->m_TextureBuffer = texture;
  //
  //   WGPUTextureViewDescriptor textureViewDesc;
  //   textureViewDesc.label = "nn";
  //   textureViewDesc.aspect = WGPUTextureAspect_All;
  //   textureViewDesc.baseArrayLayer = 0;
  //   textureViewDesc.arrayLayerCount = 6;
  //   textureViewDesc.baseMipLevel = 0;
  //   textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
  //   textureViewDesc.dimension = WGPUTextureViewDimension_Cube;
  //   textureViewDesc.format = textureDesc.format;
  //   textureViewDesc.nextInChain = nullptr;
  //   //tex->View = ;
  //	tex->m_Views.push_back(wgpuTextureCreateView(texture, &textureViewDesc));
  //
  //   return tex;
  // }
}  // namespace Rain
