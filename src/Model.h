#pragma once
#include <vector>
#include "Mesh.h"
#include "Node.h"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

class Model : public Node {
 public:
  Model(const char* path, WGPUBindGroupLayout& resourceLayout, WGPUDevice& device,WGPUQueue& queue, WGPUSampler& textureSampler);

  std::vector<Ref<MeshE>> meshes;
  std::string directory;
  std::vector<std::shared_ptr<Texture>> textures_loaded;

  void loadModel(std::string path, WGPUBindGroupLayout& resourceLayout, WGPUDevice& device, WGPUQueue& queue, WGPUSampler& textureSampler);
  void processNode(aiNode* node, const aiScene* scene, WGPUBindGroupLayout& resourceLayout, WGPUDevice& device, WGPUQueue& queue, WGPUSampler& textureSampler);
  Ref<MeshE> processMesh(aiMesh* mesh, const aiScene* scene, WGPUBindGroupLayout& resourceLayout, WGPUDevice& device, WGPUQueue& queue,  WGPUSampler& textureSampler);
  std::shared_ptr<Texture> loadMaterialTexture(aiMaterial* mat, aiTextureType type, std::string typeName);

  void Draw(WGPURenderPassEncoder& renderPass, WGPURenderPipeline& pipeline);

 private:
  std::string strPath;
};
