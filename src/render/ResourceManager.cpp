#include "ResourceManager.h"
#include <stb_image.h>
#include <stb_image_resize2.h>
#include <iostream>
#include "core/Assert.h"
#include "core/Log.h"
#include "debug/Profiler.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#ifndef STB_IMAGE_RESIZE2_IMPLEMENTATION
#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#endif
#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#endif

namespace WebEngine
{
  std::unordered_map<std::string, std::shared_ptr<Texture2D>> WebEngine::ResourceManager::m_LoadedTextures;
  std::unordered_map<std::string, std::shared_ptr<TextureCube>> WebEngine::ResourceManager::_loadedTexturesCube;

  std::unordered_map<AssetHandle, Ref<MeshSource>> WebEngine::ResourceManager::m_LoadedMeshSources;
  std::unordered_map<std::string, AssetHandle> WebEngine::ResourceManager::m_LoadedMeshPaths;
  std::unordered_map<std::string, std::string> WebEngine::ResourceManager::m_LoadedTexturePaths;

  bool WebEngine::ResourceManager::IsTextureExist(const std::string& id)
  {
    return m_LoadedTextures.find(id) != m_LoadedTextures.end();
  }

  // TOOD: check if file exist or not
  std::shared_ptr<Texture2D> WebEngine::ResourceManager::LoadTexture(const std::string& id, const std::string& path)
  {
    TextureProps defaultProps = {};
    defaultProps.DebugName = id;
    defaultProps.CreateSampler = true;
    defaultProps.GenerateMips = true;
    return LoadTexture(id, path, defaultProps);
  }

  std::shared_ptr<Texture2D> WebEngine::ResourceManager::LoadTexture(const std::string& id, const std::string& path, const TextureProps& props)
  {
    RN_PROFILE_FUNC;

    if (m_LoadedTexturePaths.contains(path))
    {
      return m_LoadedTextures[m_LoadedTexturePaths[path]];
    };

    TextureProps textureProp = props;
    textureProp.DebugName = id;
    textureProp.CreateSampler = true;
    auto texture = Texture2D::Create(textureProp, std::filesystem::path(path));

    m_LoadedTextures[id] = texture;

    RN_LOG("Texture {} loaded from {} (wrap={}, filter={})", id, path,
           static_cast<int>(textureProp.SamplerWrap), static_cast<int>(textureProp.SamplerFilter));
    return texture;
  }

  std::shared_ptr<Texture2D> WebEngine::ResourceManager::LoadTextureFromMemory(
      const std::string& id, const TextureProps& props, Buffer imageData)
  {
    auto texture = Texture2D::CreateFromMemory(props, imageData);
    m_LoadedTextures[id] = texture;
    RN_LOG("Embedded KTX2 texture '{}' loaded ({}x{})", id, props.Width, props.Height);
    return texture;
  }

  void WebEngine::ResourceManager::RegisterTexture(const std::string& id, const std::shared_ptr<Texture2D>& texture)
  {
    m_LoadedTextures[id] = texture;
    RN_LOG("Texture '{}' registered ({}x{})", id, texture->GetWidth(), texture->GetHeight());
  }

  std::shared_ptr<Texture2D> WebEngine::ResourceManager::GetTexture(const std::string& id)
  {
    if (m_LoadedTextures.find(id) == m_LoadedTextures.end())
    {
      std::cout << "GetTexture for id " << id << " does not exist" << '\n';
      return nullptr;
    }

    std::shared_ptr<Texture2D>& texture = m_LoadedTextures[id];

    if (!texture)
    {
      std::cout << "Texture is invalid" << '\n';
    }

    return texture;
  }

  Ref<MeshSource> WebEngine::ResourceManager::GetMeshSource(const UUID& handle)
  {
    RN_ASSERT(m_LoadedMeshSources.find(handle) != m_LoadedMeshSources.end());
    return m_LoadedMeshSources[handle];
  }

  Ref<MeshSource> WebEngine::ResourceManager::LoadMeshSource(const std::string& path)
  {
    if (m_LoadedMeshPaths.contains(path))
    {
      return m_LoadedMeshSources[m_LoadedMeshPaths[path]];
    }

    Ref<MeshSource> meshSource = CreateRef<MeshSource>(path);
    m_LoadedMeshSources[meshSource->Id] = meshSource;
    return meshSource;
  }
}  // namespace WebEngine
