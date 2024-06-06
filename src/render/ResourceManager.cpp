#include "ResourceManager.h"
#include <fstream>
#include <iostream>
#include <vector>
#include "../stb_image.h"
#include "core/Assert.h"

std::shared_ptr<WGPUDevice> Rain::ResourceManager::m_device;

std::unordered_map<std::string, std::shared_ptr<Texture>> Rain::ResourceManager::_loadedTextures;
std::unordered_map<std::string, std::shared_ptr<WGPUShaderModule>> Rain::ResourceManager::_loadedShaders;
std::unordered_map<AssetHandle, Ref<MeshSource>> Rain::ResourceManager::m_LoadedMeshSources;

static UUID nextUUID = 0;

WGPUShaderModule loadShaderModule(const std::string& path, WGPUDevice m_device) {
  std::cout << "SSS: " << path;
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "Error opening file '" << path << "': ";
    std::cerr << strerror(errno) << std::endl;
    return nullptr;
  }
  file.seekg(0, std::ios::end);
  size_t size = file.tellg();
  std::string shaderSource(size, ' ');
  file.seekg(0);
  file.read(shaderSource.data(), size);

  std::cout << "shader src: " << shaderSource.c_str();
  WGPUShaderModuleWGSLDescriptor shaderCodeDesc;
  shaderCodeDesc.chain.next = nullptr;
  shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
  shaderCodeDesc.code = shaderSource.c_str();
  WGPUShaderModuleDescriptor shaderDesc;
  shaderDesc.nextInChain = &shaderCodeDesc.chain;

  return wgpuDeviceCreateShaderModule(m_device, &shaderDesc);
}

void writeMipMaps(
    WGPUDevice m_device,
    WGPUTexture m_texture,
    WGPUExtent3D textureSize,
    uint32_t mipLevelCount,
    const unsigned char* pixelData) {
  WGPUQueue m_queue = wgpuDeviceGetQueue(m_device);

  // Arguments telling which part of the texture we upload to
  WGPUImageCopyTexture destination = {};
  destination.texture = m_texture;
  destination.origin = {0, 0, 0};
  destination.aspect = WGPUTextureAspect_All;

  // Arguments telling how the pixel memory is laid out
  WGPUTextureDataLayout source = {};
  source.offset = 0;

  WGPUExtent3D mipLevelSize = textureSize;
  std::vector<unsigned char> previousLevelPixels;
  WGPUExtent3D previousMipLevelSize;
  for (uint32_t level = 0; level < mipLevelCount; ++level) {
    std::vector<unsigned char> pixels(4 * mipLevelSize.width * mipLevelSize.height);
    if (level == 0) {
      std::memcpy(pixels.data(), pixelData, pixels.size());
    } else {
      for (uint32_t i = 0; i < mipLevelSize.width; ++i) {
        for (uint32_t j = 0; j < mipLevelSize.height; ++j) {
          unsigned char* p = &pixels[4 * (j * mipLevelSize.width + i)];
          unsigned char* p00 = &previousLevelPixels[4 * ((2 * j + 0) * previousMipLevelSize.width + (2 * i + 0))];
          unsigned char* p01 = &previousLevelPixels[4 * ((2 * j + 0) * previousMipLevelSize.width + (2 * i + 1))];
          unsigned char* p10 = &previousLevelPixels[4 * ((2 * j + 1) * previousMipLevelSize.width + (2 * i + 0))];
          unsigned char* p11 = &previousLevelPixels[4 * ((2 * j + 1) * previousMipLevelSize.width + (2 * i + 1))];
          p[0] = (p00[0] + p01[0] + p10[0] + p11[0]) / 4;
          p[1] = (p00[1] + p01[1] + p10[1] + p11[1]) / 4;
          p[2] = (p00[2] + p01[2] + p10[2] + p11[2]) / 4;
          p[3] = (p00[3] + p01[3] + p10[3] + p11[3]) / 4;
        }
      }
    }

    destination.mipLevel = level;
    source.bytesPerRow = 4 * mipLevelSize.width;
    source.rowsPerImage = mipLevelSize.height;
    wgpuQueueWriteTexture(m_queue, &destination, pixels.data(), pixels.size(), &source, &mipLevelSize);

    previousLevelPixels = std::move(pixels);
    previousMipLevelSize = mipLevelSize;
    mipLevelSize.width = mipLevelSize.width > 1 ? mipLevelSize.width / 2 : 1;
    mipLevelSize.height = mipLevelSize.height > 1 ? mipLevelSize.height / 2 : 1;
  }

  wgpuQueueRelease(m_queue);
}

// Equivalent of std::bit_width that is available from C++20 onward
static uint32_t bit_width(uint32_t m) {
  if (m == 0) {
    return 0;
  } else {
    uint32_t w = 0;
    while (m >>= 1) {
      ++w;
    }
    return w;
  }
}

WGPUTexture loadTexture(const char* path, std::shared_ptr<WGPUDevice> m_device, WGPUTextureView* pTextureView) {
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
  textureDesc.mipLevelCount = bit_width(std::max(textureDesc.size.width, textureDesc.size.height));  // Implement bit_width
  //textureDesc.mipLevelCount = 1;  // Implement bit_width
  textureDesc.sampleCount = 1;
  textureDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;

  WGPUTexture m_texture = wgpuDeviceCreateTexture(*m_device.get(), &textureDesc);

  // Upload Data to Texture
  // Note: Implement writeMipMaps function compatible with Dawn's C API
  writeMipMaps(*m_device, m_texture, textureDesc.size, textureDesc.mipLevelCount, pixelData);

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

void Rain::ResourceManager::Init(std::shared_ptr<WGPUDevice> device) {
  m_device = device;
}

bool Rain::ResourceManager::IsTextureExist(std::string id) {
	return _loadedTextures.find(id) != _loadedTextures.end();
}
std::shared_ptr<Texture> Rain::ResourceManager::LoadTexture(std::string id, std::string path) {
  auto tex = std::make_shared<Texture>();
  tex->Texture = loadTexture(path.c_str(), m_device, &tex->View);

  _loadedTextures[id] = tex;
  return tex;
}

std::shared_ptr<Texture> Rain::ResourceManager::GetTexture(std::string id) {
  if (_loadedTextures.find(id) == _loadedTextures.end()) {
    std::cout << "GetTexture for id " << id << " does not exist" << std::endl;
    return nullptr;
  }

  std::shared_ptr<Texture>& texture = _loadedTextures[id];

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
	meshSource->Id = nextUUID++;
	m_LoadedMeshSources[meshSource->Id] = meshSource;
	return meshSource;
}

std::shared_ptr<WGPUShaderModule> Rain::ResourceManager::GetShader(std::string id) {
  if (_loadedShaders.find(id) != _loadedShaders.end()) {
    std::cout << "GetShader for id " << id << " does not exist" << std::endl;
    return nullptr;
  }

  std::shared_ptr<WGPUShaderModule>& shader = _loadedShaders[id];

  if (!shader) {
    std::cout << "Shader is invalid" << std::endl;
  }

  return shader;
}
