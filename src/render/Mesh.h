#pragma once

#include <vector>
#include "Material.h"
#include "animation/OzzAnimation.h"
#include "animation/OzzSkeleton.h"
#include "animation/Skeleton.h"
#include "core/UUID.h"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

namespace Rain
{
  struct VertexAttribute
  {
    glm::vec3 Position;
    float _pad0;
    glm::vec3 Normal;
    float _pad1;
    glm::vec2 TexCoords;
    float _pad2[2];
    glm::vec3 Tangent;
    float _pad3;
    glm::vec3 Bitangent;
    float _pad4;
  };

  struct SkeletalVertexAttribute
  {
    glm::vec3 Position;
    float _pad0;
    glm::vec3 Normal;
    float _pad1;
    glm::vec2 TexCoords;
    float _pad2[2];
    glm::vec3 Tangent;
    float _pad3;
    glm::vec3 Bitangent;
    float _pad4;
    glm::uvec4 BoneIndices;  // 16 bytes (unsigned)
    glm::vec4 BoneWeights;   // 16 bytes
  };  // Total: 112 bytes

  struct SubMesh
  {
   public:
    uint32_t BaseVertex;
    uint32_t BaseIndex;
    uint32_t IndexCount;
    uint32_t VertexCount;
    uint32_t MaterialIndex;
  };

  class MeshNode
  {
   public:
    uint32_t Parent = 0xffffffff;
    int SubMeshId;
    std::string Name;
    glm::mat4 LocalTransform;

    inline bool IsRoot() const { return Parent == 0xffffffff; }
  };

  // TODO: At the moment we pack everything into MeshSource class, in the future we should seperate import, materials etc.
  class MeshSource
  {
   public:
    UUID Id;
    std::vector<Ref<MeshNode>> m_Nodes;
    std::vector<SubMesh> m_SubMeshes;

    std::string m_Path;
    std::string m_Directory;
    Ref<MaterialTable> Materials;

    MeshSource(const std::vector<glm::vec3>& vertices, const std::vector<glm::vec3>& indices);
    MeshSource(std::string path);

    const Ref<MeshNode> GetRootNode() const { return m_Nodes[0]; }
    const std::vector<Ref<MeshNode>> GetNodes() const { return m_Nodes; }

    Ref<GPUBuffer> GetVertexBuffer() { return m_VertexBuffer; }
    Ref<GPUBuffer> GetIndexBuffer() { return m_IndexBuffer; }
    Ref<GPUBuffer> GetSkeletalVertexBuffer() { return m_SkeletalVertexBuffer; }

    bool HasSkeleton() const { return m_Skeleton != nullptr && !m_Skeleton->Bones.empty(); }
    Ref<Skeleton> GetSkeleton() { return m_Skeleton; }

    bool HasOzzSkeleton() const { return m_OzzSkeleton != nullptr; }
    Ref<OzzSkeleton> GetOzzSkeleton() { return m_OzzSkeleton; }
    Ref<OzzAnimation> GetOzzAnimation(size_t index);
    size_t GetOzzAnimationCount() const { return m_OzzAnimations.size(); }

   private:
    Ref<GPUBuffer> m_VertexBuffer;
    Ref<GPUBuffer> m_IndexBuffer;
    Ref<GPUBuffer> m_SkeletalVertexBuffer;
    Ref<Skeleton> m_Skeleton;

    Ref<OzzSkeleton> m_OzzSkeleton;
    std::vector<Ref<OzzAnimation>> m_OzzAnimations;

    void TraverseNode(aiNode* node, const aiScene* scene);
    void ExtractBones(const aiScene* scene);
  };
}  // namespace Rain
