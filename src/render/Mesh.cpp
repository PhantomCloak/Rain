#include "Mesh.h"
#include <cstring>
#include <glm/gtx/matrix_decompose.hpp>
#include <iostream>
#include <limits>
#include "ResourceManager.h"
#include "animation/OzzConverter.h"
#include "core/KeyCode.h"
#include "core/Log.h"
#include "io/filesystem.h"
#include "render/ShaderManager.h"
#include "render/TextureImporter.h"

namespace WebEngine
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

  TextureProps GetTexturePropsFromAssimp(const aiMaterial* aiMat, aiTextureType texType, int texIndex = 0)
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

  static const uint8_t KTX2_MAGIC[12] = {
      0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

  static bool IsKTX2(const uint8_t* data, size_t size)
  {
    return size >= 12 && memcmp(data, KTX2_MAGIC, 12) == 0;
  }

  static Ref<Texture2D> LoadEmbeddedKTXTexture(
      const aiScene* scene,
      int embIndex,
      const std::string& name,
      const TextureProps& baseProps)
  {
    if (embIndex < 0 || embIndex >= static_cast<int>(scene->mNumTextures))
    {
      return nullptr;
    }

    const aiTexture* embTex = scene->mTextures[embIndex];
    if (embTex->mHeight != 0)  // mHeight==0 means compressed blob; mHeight>0 is raw pixels
    {
      return nullptr;
    }

    const auto* rawBytes = reinterpret_cast<const uint8_t*>(embTex->pcData);
    const size_t byteCount = static_cast<size_t>(embTex->mWidth);

    if (!IsKTX2(rawBytes, byteCount))
    {
      RN_LOG_ERR("Embedded texture '{}' is not KTX2, skipping.", name);
      return nullptr;
    }

    KTXImportResult ktxResult = TextureImporter::ImportKTXFromMemory(rawBytes, byteCount);

    if (!ktxResult)
    {
      RN_LOG_ERR("KTX2 decode failed for embedded texture '{}'.", name);
      return nullptr;
    }

    TextureProps props = baseProps;
    props.Width = ktxResult.baseWidth;
    props.Height = ktxResult.baseHeight;
    props.Format = ktxResult.format;
    props.DebugName = name;

    auto texture = Texture2D::CreateFromKTX(props, ktxResult);
    WebEngine::ResourceManager::RegisterTexture(name, texture);
    return texture;
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

  Ref<Texture2D> ExtractAssimpTexture(const aiScene* aiScene, const aiMaterial* aiMaterial, const std::string& assetPath, int textureSlotIndex, aiTextureType aiTexType)
  {
    aiString texturePath;
    if (aiMaterial->GetTexture(aiTexType, textureSlotIndex, &texturePath) != aiReturn_SUCCESS)
    {
      std::cout << "An error occured while loading texture" << '\n';
      return {};
    }

    const std::string textNameId = aiMaterial->GetName().C_Str() + std::string(texturePath.C_Str());

    if (WebEngine::ResourceManager::IsTextureExist(textNameId))
    {
      return WebEngine::ResourceManager::GetTexture(textNameId);
    }

    const char* texPathStr = texturePath.C_Str();
    const bool bIsEmbeddedTexture = texPathStr[0] == '*';

    TextureProps texProps = GetTexturePropsFromAssimp(aiMaterial, aiTexType, textureSlotIndex);

    if (bIsEmbeddedTexture)
    {
      int embIndex = std::atoi(texPathStr + 1);
      std::string embName = aiMaterial->GetName().C_Str() + std::string("_emb_diff_") + std::to_string(embIndex);

      return LoadEmbeddedKTXTexture(aiScene, embIndex, embName, texProps);
    }
    else
    {
      std::string path = FileSys::GetFileName(texPathStr);
      std::string rootPath = FileSys::GetParentDirectory(assetPath);
      return WebEngine::ResourceManager::LoadTexture(textNameId, rootPath + "/" + path, texProps);
    }
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
      ai_real metallicFactor = 0.0f;
      ai_real roughnessFactor = 0.8f;

      std::string aiMatName(aiMat->GetName().C_Str());
      static auto defaultShader = ShaderManager::GetShader("SH_DefaultBasicBatch");

      auto material = Material::CreateMaterial(aiMatName, defaultShader);
      material->Set("Metallic", metallicFactor);
      material->Set("Roughness", roughnessFactor);
      material->Set("Ao", 0.5f);

      assert(aiMat->GetTextureCount(aiTextureType_DIFFUSE) <= 1);
      assert(aiMat->GetTextureCount(aiTextureType_NORMALS) <= 1);
      assert(aiMat->GetTextureCount(aiTextureType_METALNESS) <= 1);

      if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) > 0)
      {
        if (const auto texture = ExtractAssimpTexture(scene, aiMat, path, 0, aiTextureType_DIFFUSE); texture != nullptr)
        {
          material->Set("u_AlbedoTex", texture);
          material->Set("u_TextureSampler", texture->Sampler);
        }
      }

      if (aiMat->GetTextureCount(aiTextureType_NORMALS) > 0)
      {
        if (const auto texture = ExtractAssimpTexture(scene, aiMat, path, 0, aiTextureType_NORMALS); texture != nullptr)
        {
          material->Set("u_NormalTex", texture);
          material->Set("UseNormalMap", true);
        }
      }
      else
      {
        material->Set("UseNormalMap", false);
      }

      // TODO: Implement the case where metallic not exist
      if (aiMat->GetTextureCount(aiTextureType_METALNESS) > 0)
      {
        if (const auto texture = ExtractAssimpTexture(scene, aiMat, path, 0, aiTextureType_METALNESS); texture != nullptr)
        {
          material->Set("u_MetallicTex", texture);
        }
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

      glm::vec3 boundsMin(std::numeric_limits<float>::max());
      glm::vec3 boundsMax(std::numeric_limits<float>::lowest());

      for (int j = 0; j < mesh->mNumVertices; j++)
      {
        VertexAttribute vertex;
        glm::vec3 vector;
        vector.x = mesh->mVertices[j].x;
        vector.y = mesh->mVertices[j].y;
        vector.z = mesh->mVertices[j].z;
        vertex.Position = vector;

        boundsMin = glm::min(boundsMin, vertex.Position);
        boundsMax = glm::max(boundsMax, vertex.Position);

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

      subMesh.BoundsMin = boundsMin;
      subMesh.BoundsMax = boundsMax;

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

    // Sort bones so parents come before children, then compute matrices
    m_Skeleton->SortBones();
    m_Skeleton->ComputeBoneMatrices();

    RN_LOG("Loaded skeleton with {} bones", m_Skeleton->Bones.size());

    // Debug: print bone hierarchy
    for (size_t i = 0; i < m_Skeleton->Bones.size(); i++)
    {
      const auto& bone = m_Skeleton->Bones[i];
      RN_LOG("Bone[{}]: {} (parent: {})", i, bone.Name, bone.ParentIndex);
    }

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

    // Convert to ozz-animation format if animations are present
    if (scene->mNumAnimations > 0)
    {
      m_OzzSkeleton = OzzConverter::ConvertSkeleton(scene, *m_Skeleton);
      if (m_OzzSkeleton)
      {
        for (uint32_t i = 0; i < scene->mNumAnimations; ++i)
        {
          auto ozzAnim = OzzConverter::ConvertAnimation(scene->mAnimations[i], *m_OzzSkeleton);
          if (ozzAnim)
          {
            m_OzzAnimations.push_back(ozzAnim);
          }
        }
        RN_LOG("Converted {} animations to ozz format", m_OzzAnimations.size());
      }
    }
  }

  Ref<OzzAnimation> MeshSource::GetOzzAnimation(size_t index)
  {
    if (index < m_OzzAnimations.size())
    {
      return m_OzzAnimations[index];
    }
    return nullptr;
  }
}  // namespace WebEngine
