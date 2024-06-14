#pragma once
#include "core/UUID.h"
#define STB_IMAGE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION

#include <webgpu/webgpu.h>
#include <string>
#include <unordered_map>
#include "render/Mesh.h"
#include "render/Texture.h"

typedef UUID AssetHandle;

namespace Rain {
  class ResourceManager {
   public:
    static void Init(std::shared_ptr<WGPUDevice> device);

    static std::shared_ptr<Texture> LoadTexture(std::string id, std::string path);
    static Ref<MeshSource> GetMeshSource(UUID handle);
    static Ref<MeshSource> LoadMeshSource(std::string path);

    static std::shared_ptr<Texture> GetTexture(std::string id);
    static bool IsTextureExist(std::string id);
    static std::shared_ptr<WGPUShaderModule> GetShader(std::string id);

   private:
    static std::unordered_map<std::string, std::shared_ptr<Texture>> _loadedTextures;
    static std::unordered_map<std::string, std::shared_ptr<WGPUShaderModule>> _loadedShaders;
    static std::unordered_map<AssetHandle, Ref<MeshSource>> m_LoadedMeshSources;
    static std::shared_ptr<WGPUDevice> m_device;
  };
}  // namespace Rain
