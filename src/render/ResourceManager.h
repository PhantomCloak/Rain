#pragma once
#include <webgpu/webgpu.h>
#include <string>
#include <unordered_map>
#include "core/UUID.h"
#include "render/Mesh.h"
#include "render/Texture.h"

namespace Rain {
  typedef UUID AssetHandle;

  class ResourceManager {
   public:
    static std::shared_ptr<Texture2D> LoadTexture(std::string id, std::string path);
    // static std::shared_ptr<Texture2D> LoadCubeTexture(std::string id, const std::string (&paths)[6]);
    static std::shared_ptr<TextureCube> LoadCubeTexture(std::string id, const std::filesystem::path (&paths)[6]);
    static Ref<MeshSource> GetMeshSource(UUID handle);
    static Ref<MeshSource> LoadMeshSource(std::string path);

    static std::shared_ptr<Texture2D> GetTexture(std::string id);
    static bool IsTextureExist(std::string id);

   private:
    static std::unordered_map<std::string, std::shared_ptr<Texture2D>> _loadedTextures;
    static std::unordered_map<std::string, std::shared_ptr<TextureCube>> _loadedTexturesCube;
    static std::unordered_map<AssetHandle, Ref<MeshSource>> m_LoadedMeshSources;
  };
}  // namespace Rain
