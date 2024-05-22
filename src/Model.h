#pragma once
#include <vector>
#include "Mesh.h"
#include "Node.h"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

class Model {
 public:
	 Model(const char* path, MaterialUniform mat, WGPUSampler& textureSampler);

  std::vector<Ref<Mesh>> meshes;
  std::string directory;
  std::vector<std::shared_ptr<Texture>> textures_loaded; // Remove

	MaterialUniform materialOverride;

  void loadModel(std::string path, WGPUSampler& textureSampler);
  void processNode(aiNode* node, const aiScene* scene, WGPUSampler& textureSampler);
  Ref<Mesh> processMesh(aiMesh* mesh, const aiScene* scene,  WGPUSampler& textureSampler);
  std::shared_ptr<Texture> loadMaterialTexture(aiMaterial* mat, aiTextureType type, std::string typeName);

  void Draw(WGPURenderPassEncoder& renderPass, WGPURenderPipeline& pipeline);
	void UpdateUniforms(WGPUQueue& queue);

 private:
  std::string strPath;
};
