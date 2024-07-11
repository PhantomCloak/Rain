#pragma once

#include <vector>
#include "Material.h"
#include "core/UUID.h"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

struct VertexAttribute {
  glm::vec3 Position;
  float _pad0;
  glm::vec3 Normal;
  float _pad1;
  glm::vec2 TexCoords;
  float _pad2[2];
  glm::vec3 Tangent;
  float _pad3;
};

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

// TODO: At the moment we pack everything into MeshSource class, in the future we should seperate import, materials etc.
class MeshSource {
 public:
  UUID Id = 0;
  std::vector<Ref<MeshNode>> m_Nodes;
  std::vector<SubMesh> m_SubMeshes;

  std::string m_Path;
  std::string m_Directory;
  std::vector<Ref<Material>> m_Materials;

  MeshSource(const std::vector<glm::vec3>& vertices, const std::vector<glm::vec3>& indices);
  MeshSource(std::string path);

  const Ref<MeshNode> GetRootNode() const { return m_Nodes[0]; }
  const std::vector<Ref<MeshNode>> GetNodes() const { return m_Nodes; }

  Ref<GPUBuffer> GetVertexBuffer() { return m_VertexBuffer; }
  Ref<GPUBuffer> GetIndexBuffer() { return m_IndexBuffer; }

 private:
  Ref<GPUBuffer> m_VertexBuffer;
  Ref<GPUBuffer> m_IndexBuffer;
  void TraverseNode(aiNode* node, const aiScene* scene);
};
