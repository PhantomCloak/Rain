#pragma once
#include <map>
#include <vector>
#include "Mesh.h"
#include "Node.h"
#include "render/GPUAllocator.h"
#include "Material.h"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

static int modelNextId = 0;
class Model {
 public:
  UUID Id;
  Model(const char* path, WGPUSampler& textureSampler);

  std::vector<Ref<Mesh>> meshes;
  std::string directory;
  std::vector<std::shared_ptr<Texture>> textures_loaded;  // Remove

  void loadModel(std::string path, WGPUSampler& textureSampler);
  void processNode(aiNode* node, const aiScene* scene, WGPUSampler& textureSampler);
  Ref<Mesh> processMesh(aiMesh* mesh, const aiScene* scene, WGPUSampler& textureSampler);
  std::shared_ptr<Texture> loadMaterialTexture(aiMaterial* mat, aiTextureType type, std::string typeName);

  WGPUBuffer vertexBuffer;
  WGPUBuffer indexBuffer;
  int idcCtx;
  int vtCtx;

 private:
  std::string strPath;
};

struct SubMesh {
 public:
  uint32_t BaseVertex;
  uint32_t BaseIndex;
  uint32_t IndexCount;
  uint32_t VertexCount;
  uint32_t MaterialIndex;

  uint32_t DrawCount = 0;
};

class MeshNode {
 public:
  int SubMeshId;
  std::string Name;
  glm::mat4 LocalTransform;
};

class MeshSource {
 public:
	UUID Id;
  std::vector<Ref<MeshNode>> m_Nodes;
  std::vector<SubMesh> m_SubMeshes;

  std::string m_Path;
  std::string m_Directory;
	std::vector<Ref<MaterialEngine>> m_Materials;

  MeshSource(std::string path);

	GPUBuffer vertexBuffer;
	GPUBuffer indexBuffer;
  void TraverseNode(aiNode* node, const aiScene* scene);
};
