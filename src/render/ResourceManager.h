#pragma once
#include <webgpu/webgpu.h>
#include <string>
#include <unordered_map>
#include "core/UUID.h"
#include "render/Mesh.h"
#include "render/Texture.h"

namespace WebEngine
{
  typedef UUID AssetHandle;

  class Asset
  {
    UUID Handle = 0;

    virtual bool operator==(const Asset& other) const
    {
      return Handle == other.Handle;
    }
  };

  class ResourceManager
  {
   public:
    static std::shared_ptr<Texture2D> LoadTexture(const std::string& id, const std::string& path);
    static std::shared_ptr<Texture2D> LoadTexture(const std::string& id, const std::string& path, const TextureProps& props);
    static std::shared_ptr<Texture2D> LoadTextureFromMemory(const std::string& id, const TextureProps& props, Buffer imageData);
    static Ref<MeshSource> GetMeshSource(const UUID& handle);
    static Ref<MeshSource> LoadMeshSource(const std::string& path);

    static void RegisterTexture(const std::string& id, const std::shared_ptr<Texture2D>& texture);
    static std::shared_ptr<Texture2D> GetTexture(const std::string& id);
    static bool IsTextureExist(const std::string& id);

   private:
    static std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_LoadedTextures;
    static std::unordered_map<std::string, std::shared_ptr<TextureCube>> _loadedTexturesCube;
    static std::unordered_map<AssetHandle, Ref<MeshSource>> m_LoadedMeshSources;

    static std::unordered_map<std::string, AssetHandle> m_LoadedMeshPaths;
    static std::unordered_map<std::string, std::string> m_LoadedTexturePaths;
  };
}  // namespace WebEngine
