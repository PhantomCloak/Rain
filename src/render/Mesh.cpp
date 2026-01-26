#include "Mesh.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <iostream>
#include "ResourceManager.h"
#include "core/KeyCode.h"
#include "core/Log.h"
#include "io/filesystem.h"
#include "render/ShaderManager.h"

namespace Rain
{
  TextureWrappingFormat ConvertAssimpWrapMode(aiTextureMapMode mode)
  {
    switch (mode)
    {
      case aiTextureMapMode_Wrap:
        return TextureWrappingFormat::Repeat;
      case aiTextureMapMode_Clamp:
        return TextureWrappingFormat::ClampToEdges;
      case aiTextureMapMode_Mirror:
        return TextureWrappingFormat::Repeat;
      case aiTextureMapMode_Decal:
        return TextureWrappingFormat::ClampToEdges;
      default:
        return TextureWrappingFormat::Repeat;
    }
  }

  TextureProps GetTexturePropsFromAssimp(aiMaterial* aiMat, aiTextureType texType, int texIndex = 0)
  {
    TextureProps props = {};
    props.CreateSampler = true;
    props.GenerateMips = true;
    props.SamplerFilter = FilterMode::Linear;
    props.SamplerWrap = TextureWrappingFormat::Repeat;

    aiTextureMapMode wrapU = aiTextureMapMode_Wrap;
    aiTextureMapMode wrapV = aiTextureMapMode_Wrap;

    if (aiMat->Get(AI_MATKEY_MAPPINGMODE_U(texType, texIndex), wrapU) == AI_SUCCESS)
    {
      props.SamplerWrap = ConvertAssimpWrapMode(wrapU);
    }
    if (aiMat->Get(AI_MATKEY_MAPPINGMODE_V(texType, texIndex), wrapV) == AI_SUCCESS)
    {
      if (ConvertAssimpWrapMode(wrapV) == TextureWrappingFormat::ClampToEdges)
      {
        props.SamplerWrap = TextureWrappingFormat::ClampToEdges;
      }
    }

    return props;
  }

  glm::mat4 convertToGLM(const aiMatrix4x4& from)
  {
    glm::mat4 to;

    to[0][0] = from.a1;
    to[0][1] = from.b1;
    to[0][2] = from.c1;
    to[0][3] = from.d1;
    to[1][0] = from.a2;
    to[1][1] = from.b2;
    to[1][2] = from.c2;
    to[1][3] = from.d2;
    to[2][0] = from.a3;
    to[2][1] = from.b3;
    to[2][2] = from.c3;
    to[2][3] = from.d3;
    to[3][0] = from.a4;
    to[3][1] = from.b4;
    to[3][2] = from.c4;
    to[3][3] = from.d4;

    return to;
  }

  MeshSource::MeshSource(std::string path)
  {
    RN_ASSERT(FileSys::IsFileExist(path), "MeshSource: The file does not exist at the specified path.");

    Assimp::Importer import;
    const aiScene* scene = import.ReadFile(path,
                                           aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
      std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
      return;
    }

    RN_CORE_ASSERT(scene && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || scene->mRootNode, "An error occured while loading the model %s", import.GetErrorString());

    uint32_t offsetVertex = 0;
    uint32_t offsetIndex = 0;

    int verticesCount = 0;
    int indexCount = 0;

    for (int i = 0; i < scene->mNumMeshes; i++)
    {
      aiMesh* mesh = scene->mMeshes[i];
      verticesCount += mesh->mNumVertices;

      for (int j = 0; j < mesh->mNumFaces; j++)
      {
        indexCount += mesh->mFaces[j].mNumIndices;
      }
    }

    std::string fileName = FileSys::GetFileName(path);
    std::string fileDirectory = FileSys::GetParentDirectory(path);
    Materials = CreateRef<MaterialTable>();

    m_VertexBuffer = GPUAllocator::GAlloc("v_buffer_" + fileName, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex, (verticesCount * sizeof(VertexAttribute) + 3) & ~3);
    m_IndexBuffer = GPUAllocator::GAlloc("i_buffer_" + fileName, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index, (indexCount * sizeof(unsigned int) + 3) & ~3);

    aiColor3D colorEmpty = {0, 0, 0};

    for (int i = 0; i < scene->mNumMaterials; i++)
    {
      aiMaterial* aiMat = scene->mMaterials[i];
      ai_real metallicFactor = 0.5f;
      ai_real roughnessFactor = 0.5f;

      if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallicFactor) != AI_SUCCESS)
      {
        metallicFactor = 0.5f;  // Fallback if not specified
      }

      if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessFactor) != AI_SUCCESS)
      {
        roughnessFactor = 0.5f;  // Fallback if not specified
      }

      std::string aiMatName(aiMat->GetName().C_Str());
      static auto defaultShader = ShaderManager::GetShader("SH_DefaultBasicBatch");

      auto material = Material::CreateMaterial(aiMatName, defaultShader);
      material->Set("Metallic", metallicFactor);
      material->Set("Roughness", roughnessFactor);
      material->Set("Ao", 0.5f);

      for (int j = 0; j < aiMat->GetTextureCount(aiTextureType_DIFFUSE); j++)
      {
        aiString texturePath;
        if (aiMat->GetTexture(aiTextureType_DIFFUSE, j, &texturePath) != aiReturn_SUCCESS)
        {
          std::cout << "An error occured while loading texture" << std::endl;
          continue;
        }

        std::string textureName = FileSys::GetFileName(texturePath.C_Str());
        RN_LOG("Texture Name: {}", textureName);

        Ref<Texture2D> matTexture;
        if (Rain::ResourceManager::IsTextureExist(textureName))
        {
          matTexture = Rain::ResourceManager::GetTexture(textureName);
        }
        else
        {
          TextureProps texProps = GetTexturePropsFromAssimp(aiMat, aiTextureType_DIFFUSE, j);
          matTexture = Rain::ResourceManager::LoadTexture(textureName, fileDirectory + "/" + texturePath.C_Str(), texProps);
        }

        material->Set("u_AlbedoTex", matTexture);
        material->Set("u_TextureSampler", matTexture->Sampler);
      }

      if (aiMat->GetTextureCount(aiTextureType_NORMALS) > 0)
      {
        aiString texturePath;
        if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &texturePath) != aiReturn_SUCCESS)
        {
          std::cout << "An error occured while loading texture" << std::endl;
          continue;
        }

        std::string textureName = FileSys::GetFileName(texturePath.C_Str());

        Ref<Texture2D> matTexture;
        if (Rain::ResourceManager::IsTextureExist(textureName))
        {
          matTexture = Rain::ResourceManager::GetTexture(textureName);
        }
        else
        {
          TextureProps texProps = GetTexturePropsFromAssimp(aiMat, aiTextureType_NORMALS, 0);
          matTexture = Rain::ResourceManager::LoadTexture(textureName, fileDirectory + "/" + texturePath.C_Str(), texProps);
        }

        material->Set("u_NormalTex", matTexture);
        material->Set("UseNormalMap", true);
      }
      else
      {
        material->Set("UseNormalMap", false);
      }

      if (aiMat->GetTextureCount(aiTextureType_METALNESS) > 0)
      {
        aiString texturePath;
        if (aiMat->GetTexture(aiTextureType_METALNESS, 0, &texturePath) != aiReturn_SUCCESS)
        {
          std::cout << "An error occured while loading texture" << std::endl;
          continue;
        }

        std::string textureName = FileSys::GetFileName(texturePath.C_Str());

        Ref<Texture2D> matTexture;
        if (Rain::ResourceManager::IsTextureExist(textureName))
        {
          matTexture = Rain::ResourceManager::GetTexture(textureName);
        }
        else
        {
          TextureProps texProps = GetTexturePropsFromAssimp(aiMat, aiTextureType_METALNESS, 0);
          matTexture = Rain::ResourceManager::LoadTexture(textureName, fileDirectory + "/" + texturePath.C_Str(), texProps);
        }
        material->Set("u_MetallicTex", matTexture);
      }

      material->Bake();
      Materials->SetMaterial(i, material);
    }

    m_SubMeshes.resize(scene->mNumMeshes);

    for (int i = 0; i < scene->mNumMeshes; i++)
    {
      aiMesh* mesh = scene->mMeshes[i];

      std::vector<VertexAttribute> vertices;
      std::vector<unsigned int> indices;

      for (int j = 0; j < mesh->mNumVertices; j++)
      {
        VertexAttribute vertex;
        glm::vec3 vector;
        vector.x = mesh->mVertices[j].x;
        vector.y = mesh->mVertices[j].y;
        vector.z = mesh->mVertices[j].z;
        vertex.Position = vector;

        if (mesh->HasNormals())
        {
          vector.x = mesh->mNormals[j].x;
          vector.y = mesh->mNormals[j].y;
          vector.z = mesh->mNormals[j].z;
          vertex.Normal = vector;
        }

        if (mesh->HasTangentsAndBitangents())
        {
          vector.x = mesh->mTangents[j].x;
          vector.y = mesh->mTangents[j].y;
          vector.z = mesh->mTangents[j].z;
          vertex.Tangent = vector;

          vector.x = mesh->mBitangents[j].x;
          vector.y = mesh->mBitangents[j].y;
          vector.z = mesh->mBitangents[j].z;
          vertex.Bitangent = vector;
        }

        if (mesh->mTextureCoords[0])
        {
          glm::vec2 vec;
          vec.x = mesh->mTextureCoords[0][j].x;
          vec.y = mesh->mTextureCoords[0][j].y;
          vertex.TexCoords = vec;
        }
        else
        {
          vertex.TexCoords = glm::vec2(0.0f, 0.0f);
        }
        vertices.push_back(vertex);
      }

      for (unsigned int i = 0; i < mesh->mNumFaces; i++)
      {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
        {
          indices.push_back(face.mIndices[j]);
        }
      }

      uint32_t vertexBufferSize = vertices.size() * sizeof(VertexAttribute);
      uint32_t indexBufferSize = indices.size() * sizeof(unsigned int);

      m_VertexBuffer->SetData(vertices.data(), offsetVertex * sizeof(VertexAttribute), vertices.size() * sizeof(VertexAttribute));
      m_IndexBuffer->SetData(indices.data(), offsetIndex * sizeof(unsigned int), indices.size() * sizeof(unsigned int));

      SubMesh subMesh;
      subMesh.MaterialIndex = mesh->mMaterialIndex;
      subMesh.BaseVertex = offsetVertex;
      subMesh.VertexCount = vertices.size();

      subMesh.BaseIndex = offsetIndex;
      subMesh.IndexCount = indices.size();

      offsetVertex += vertices.size();
      offsetIndex += indices.size();

      m_SubMeshes[i] = subMesh;
    }

    int s = 0;
    int i = 0;
    for (auto& m : m_SubMeshes)
    {
      s += m.VertexCount;
      i += m.IndexCount;
    }

    TraverseNode(scene->mRootNode, scene);

    // Extract bone data if present
    ExtractBones(scene);
  }
  void DecomposeTransform(const glm::mat4& transform, glm::vec3& outPosition, glm::quat& outRotation, glm::vec3& outScale)
  {
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(transform, outScale, outRotation, outPosition, skew, perspective);
  }

  void MeshSource::TraverseNode(aiNode* node, const aiScene* scene)
  {
    if (node->mNumMeshes > 0)
    {
      Ref<MeshNode> meshNode = CreateRef<MeshNode>();
      meshNode->Parent = m_Nodes.size() == 0 ? meshNode->Parent : m_Nodes.size() - 1;
      meshNode->Name = std::string(node->mName.C_Str());
      meshNode->SubMeshId = node->mMeshes[0];  // For now just handle first mesh
      meshNode->LocalTransform = convertToGLM(node->mTransformation);

      // m_SubMeshes[meshNode->SubMeshId].DrawCount++;
      glm::vec3 position, scale;
      glm::quat rotation;
      DecomposeTransform(meshNode->LocalTransform, position, rotation, scale);
      m_Nodes.push_back(meshNode);
    }

    for (int i = 0; i < node->mNumChildren; i++)
    {
      TraverseNode(node->mChildren[i], scene);
    }
  }

  // Helper to find bone node in scene hierarchy and extract transforms
  static void FindBoneNodes(aiNode* node, Skeleton* skeleton, aiNode* parent, std::unordered_map<std::string, aiNode*>& boneNodes)
  {
    std::string nodeName(node->mName.C_Str());

    // Check if this node is a bone
    if (skeleton->BoneNameToIndex.find(nodeName) != skeleton->BoneNameToIndex.end())
    {
      boneNodes[nodeName] = node;
    }

    // Recurse into children
    for (uint32_t i = 0; i < node->mNumChildren; i++)
    {
      FindBoneNodes(node->mChildren[i], skeleton, node, boneNodes);
    }
  }

  void MeshSource::ExtractBones(const aiScene* scene)
  {
    // Check if any mesh has bones
    bool hasBones = false;
    uint32_t totalVertices = 0;

    for (uint32_t i = 0; i < scene->mNumMeshes; i++)
    {
      aiMesh* mesh = scene->mMeshes[i];
      totalVertices += mesh->mNumVertices;
      if (mesh->mNumBones > 0)
      {
        hasBones = true;
      }
    }

    if (!hasBones)
    {
      return;
    }

    // Create skeleton
    m_Skeleton = CreateRef<Skeleton>();

    // First pass: collect all unique bones with inverse bind matrices
    for (uint32_t meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++)
    {
      aiMesh* mesh = scene->mMeshes[meshIdx];

      for (uint32_t boneIdx = 0; boneIdx < mesh->mNumBones; boneIdx++)
      {
        aiBone* bone = mesh->mBones[boneIdx];
        std::string boneName(bone->mName.C_Str());

        if (m_Skeleton->BoneNameToIndex.find(boneName) == m_Skeleton->BoneNameToIndex.end())
        {
          uint32_t index = static_cast<uint32_t>(m_Skeleton->Bones.size());
          m_Skeleton->BoneNameToIndex[boneName] = index;

          Bone newBone;
          newBone.Name = boneName;
          newBone.ParentIndex = -1;
          newBone.InverseBindMatrix = convertToGLM(bone->mOffsetMatrix);

          m_Skeleton->Bones.push_back(newBone);
        }
      }
    }

    // Second pass: find bone nodes in scene hierarchy and extract transforms
    std::unordered_map<std::string, aiNode*> boneNodes;
    FindBoneNodes(scene->mRootNode, m_Skeleton.get(), nullptr, boneNodes);

    // Extract local transforms and resolve parent indices
    for (auto& bone : m_Skeleton->Bones)
    {
      auto it = boneNodes.find(bone.Name);
      if (it != boneNodes.end())
      {
        aiNode* node = it->second;
        bone.LocalTransform = convertToGLM(node->mTransformation);

        // Find parent bone
        if (node->mParent)
        {
          std::string parentName(node->mParent->mName.C_Str());
          bone.ParentIndex = m_Skeleton->GetBoneIndex(parentName);
        }
      }
    }

    // Compute final bone matrices
    m_Skeleton->ComputeBoneMatrices();

    RN_LOG("Loaded skeleton with {} bones", m_Skeleton->Bones.size());

    // Create skeletal vertex buffer with bone indices and weights
    std::vector<SkeletalVertexAttribute> skeletalVertices;
    skeletalVertices.reserve(totalVertices);

    uint32_t vertexOffset = 0;

    for (uint32_t meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++)
    {
      aiMesh* mesh = scene->mMeshes[meshIdx];

      // Initialize all vertices for this mesh
      std::vector<SkeletalVertexAttribute> meshVertices(mesh->mNumVertices);

      for (uint32_t j = 0; j < mesh->mNumVertices; j++)
      {
        SkeletalVertexAttribute& vertex = meshVertices[j];

        vertex.Position = glm::vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);

        if (mesh->HasNormals())
        {
          vertex.Normal = glm::vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);
        }

        if (mesh->HasTangentsAndBitangents())
        {
          vertex.Tangent = glm::vec3(mesh->mTangents[j].x, mesh->mTangents[j].y, mesh->mTangents[j].z);
          vertex.Bitangent = glm::vec3(mesh->mBitangents[j].x, mesh->mBitangents[j].y, mesh->mBitangents[j].z);
        }

        if (mesh->mTextureCoords[0])
        {
          vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y);
        }

        // Initialize bone data to identity (bone 0 with weight 1)
        vertex.BoneIndices = glm::uvec4(0, 0, 0, 0);
        vertex.BoneWeights = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
      }

      // Process bone weights
      for (uint32_t boneIdx = 0; boneIdx < mesh->mNumBones; boneIdx++)
      {
        aiBone* bone = mesh->mBones[boneIdx];
        std::string boneName(bone->mName.C_Str());
        int32_t boneIndex = m_Skeleton->GetBoneIndex(boneName);

        if (boneIndex < 0)
        {
          continue;
        }

        for (uint32_t weightIdx = 0; weightIdx < bone->mNumWeights; weightIdx++)
        {
          aiVertexWeight& weight = bone->mWeights[weightIdx];
          uint32_t vertexId = weight.mVertexId;
          float boneWeight = weight.mWeight;

          SkeletalVertexAttribute& vertex = meshVertices[vertexId];

          // Find first empty slot for bone weight
          for (int slot = 0; slot < 4; slot++)
          {
            if (vertex.BoneWeights[slot] == 0.0f)
            {
              vertex.BoneIndices[slot] = boneIndex;
              vertex.BoneWeights[slot] = boneWeight;
              break;
            }
          }
        }
      }

      // Normalize weights and ensure at least one bone affects each vertex
      for (auto& vertex : meshVertices)
      {
        float totalWeight = vertex.BoneWeights.x + vertex.BoneWeights.y + vertex.BoneWeights.z + vertex.BoneWeights.w;

        if (totalWeight > 0.0f)
        {
          vertex.BoneWeights /= totalWeight;
        }
        else
        {
          // No bones affect this vertex - use identity
          vertex.BoneIndices = glm::uvec4(0, 0, 0, 0);
          vertex.BoneWeights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        }
      }

      skeletalVertices.insert(skeletalVertices.end(), meshVertices.begin(), meshVertices.end());
      vertexOffset += mesh->mNumVertices;
    }

    // Create skeletal vertex buffer
    std::string fileName = FileSys::GetFileName(m_Path);
    m_SkeletalVertexBuffer = GPUAllocator::GAlloc(
        "skeletal_v_buffer_" + fileName,
        WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
        (skeletalVertices.size() * sizeof(SkeletalVertexAttribute) + 3) & ~3);
    m_SkeletalVertexBuffer->SetData(skeletalVertices.data(), skeletalVertices.size() * sizeof(SkeletalVertexAttribute));

    RN_LOG("Created skeletal vertex buffer with {} vertices", skeletalVertices.size());
  }
}  // namespace Rain
