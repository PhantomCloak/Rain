#pragma once
#define STB_IMAGE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION

#include <webgpu/webgpu.h>
#include <string>
#include <unordered_map>
#include "render/primitives/mesh.h"
#include "render/primitives/texture.h"

namespace Rain {
  class ResourceManager {
   public:
    static void Init(std::shared_ptr<WGPUDevice> device);

    static std::shared_ptr<Texture> LoadTexture(std::string id, std::string path);
    static std::shared_ptr<Mesh> LoadMesh(std::string id, const std::string& path);

    static std::shared_ptr<Texture> GetTexture(std::string id);
    static std::shared_ptr<Mesh> GetMesh(std::string id);
    static std::shared_ptr<WGPUShaderModule> GetShader(std::string id);

   private:
    static std::unordered_map<std::string, std::shared_ptr<Texture>> _loadedTextures;
    static std::unordered_map<std::string, std::shared_ptr<Mesh>> _loadedMeshes;
    static std::unordered_map<std::string, std::shared_ptr<WGPUShaderModule>> _loadedShaders;
    static std::shared_ptr<WGPUDevice> m_device;
  };
}  // namespace Rain
