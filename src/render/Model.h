#pragma once
#include <map>
#include <vector>
#include "Material.h"
#include "Mesh.h"
#include "Node.h"
#include "render/GPUAllocator.h"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

static int modelNextId = 0;
struct SubMesh {
 public:
  uint32_t BaseVertex;
  uint32_t BaseIndex;
  uint32_t IndexCount;
  uint32_t VertexCount;
  uint32_t MaterialIndex;
};

class MeshNode {
 public:
  uint32_t Parent = 0xffffffff;
  int SubMeshId;
  std::string Name;
  glm::mat4 LocalTransform;

  inline bool IsRoot() const { return Parent == 0xffffffff; }
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

  const Ref<MeshNode> GetRootNode() const { return m_Nodes[0]; }
  const std::vector<Ref<MeshNode>> GetNodes() const { return m_Nodes; }

  GPUBuffer m_VertexBuffer;
  GPUBuffer m_IndexBuffer;
  void TraverseNode(aiNode* node, const aiScene* scene);
};
