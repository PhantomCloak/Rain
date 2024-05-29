#include "Model.h"
#include <iostream>
#include <ostream>
#include "Material.h"
#include "core/Assert.h"
#include "core/Log.h"
#include "io/filesystem.h"
#include "render/Render.h"
#include "render/RenderUtils.h"
#include "render/ResourceManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_IMPLEMENTATION
#endif

int ceilToMultipleOfFour(int num) {
  return (num + 3) & ~3;  // Adds 3 and then rounds down to the nearest multiple of 4
}

unsigned int ceilToPowerOf2(unsigned int n) {
  // Handle the case where n is 0
  if (n == 0) {
    return 1;
  }

  // If n is already a power of 2, return it
  if ((n & (n - 1)) == 0) {
    return n;
  }

  // Initialize result to the highest power of 2 less than n
  unsigned int result = 1;
  while (result < n) {
    result <<= 1;
  }

  return result;
}

void Model::loadModel(std::string path, WGPUSampler& textureSampler) {
  strPath = path;

  Assimp::Importer import;
  const aiScene* scene = import.ReadFile(path,
                                         aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices);

  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
    std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
    return;
  }
  directory = path.substr(0, path.find_last_of('/'));

  processNode(scene->mRootNode, scene, textureSampler);
}

void Model::processNode(aiNode* node, const aiScene* scene, WGPUSampler& textureSampler) {
  // process all the node's meshes (if any)
  for (unsigned int i = 0; i < node->mNumMeshes; i++) {
    aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

    Ref<Mesh> modelMesh = processMesh(mesh, scene, textureSampler);
    // modelMesh->Name = mesh->mName.C_Str();
    meshes.push_back(modelMesh);
    // AddChild(modelMesh);
  }

  // then do the same for each of its children
  for (unsigned int i = 0; i < node->mNumChildren; i++) {
    processNode(node->mChildren[i], scene, textureSampler);
  }
}

Ref<Mesh> Model::processMesh(aiMesh* mesh, const aiScene* scene, WGPUSampler& textureSampler) {
  std::vector<VertexAttribute> vertices;
  std::vector<unsigned int> indices;
  std::vector<std::shared_ptr<Texture>> textures;

  for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
    VertexAttribute vertex;
    glm::vec3 vector;
    vector.x = mesh->mVertices[i].x;
    vector.y = mesh->mVertices[i].y;
    vector.z = mesh->mVertices[i].z;
    vertex.Position = vector;

    if (mesh->HasNormals()) {
      vector.x = mesh->mNormals[i].x;
      vector.y = mesh->mNormals[i].y;
      vector.z = mesh->mNormals[i].z;
      vertex.Normal = vector;
    }

    if (mesh->HasTangentsAndBitangents()) {
      vector.x = mesh->mTangents[i].x;
      vector.y = mesh->mTangents[i].y;
      vector.z = mesh->mTangents[i].z;
      vertex.Tangent = vector;
    }

    if (mesh->mTextureCoords[0]) {
      glm::vec2 vec;
      vec.x = mesh->mTextureCoords[0][i].x;
      vec.y = mesh->mTextureCoords[0][i].y;
      vertex.TexCoords = vec;
    } else {
      vertex.TexCoords = glm::vec2(0.0f, 0.0f);
    }

    vertices.push_back(vertex);
  }

  for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
    aiFace face = mesh->mFaces[i];
    for (unsigned int j = 0; j < face.mNumIndices; j++) {
      indices.push_back(face.mIndices[j]);
    }
  }

  aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
  aiColor3D color;
  float shininess;
  aiColor3D colorEmpty = {0, 0, 0};

  std::shared_ptr<Texture> diffuseTexture = loadMaterialTexture(material, aiTextureType_DIFFUSE, "texture_diffuse");
  std::shared_ptr<Texture> heightTexture = loadMaterialTexture(material, aiTextureType_DISPLACEMENT, "texture_height");

  MaterialUniform materialUniform;
  materialUniform.ambientColor = glm::vec3(0.1);
  materialUniform.diffuseColor = glm::vec3(1);
  materialUniform.specularColor = glm::vec3(1);
  materialUniform.shininess = 0.0;

  aiReturn result = AI_FAILURE;

  // MaterialEngine* currentMaterial = CreateRef<MaterialEngine>();

  // result = material->Get(AI_MATKEY_COLOR_AMBIENT, color);
  // if (result == AI_SUCCESS) {
  //	currentMaterial->properties.ambientColor = color != colorEmpty ? glm::vec3(color.r, color.g, color.b) : glm::vec3(0.1);
  //}

  // result = material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
  // if (result == AI_SUCCESS) {
  //	currentMaterial->properties.diffuseColor = glm::vec3(color.r, color.g, color.b);
  //}

  // result = material->Get(AI_MATKEY_COLOR_SPECULAR, color);
  // if (result == AI_SUCCESS) {
  //	currentMaterial->properties.specularColor = glm::vec3(color.r, color.g, color.b);
  //}

  // result = material->Get(AI_MATKEY_SHININESS, shininess);
  // if (result == AI_SUCCESS) {
  //	currentMaterial->properties.shininess = (shininess / 10.0f) * 128.0f;
  //}

  // MaterialEngine::CreateMaterial(currentMaterial);

  auto m = CreateRef<Mesh>(vertices, indices);
  return m;
}

std::shared_ptr<Texture> Model::loadMaterialTexture(aiMaterial* mat, aiTextureType type, std::string typeName) {
  static auto defaultTexture = Rain::ResourceManager::GetTexture("T_Default");

  // RN_CORE_ASSERT(defaultTexture != nullptr, "Default texture for model couldn't initialised.");
  aiString str;
  int textureCount = mat->GetTextureCount(type);
  mat->GetTexture(type, 0, &str);

  if (textureCount <= 0) {
    return defaultTexture;
  }

  for (unsigned int j = 0; j < textures_loaded.size(); j++) {
    if (std::strcmp(textures_loaded[j]->path.data(), str.C_Str()) == 0) {
      return textures_loaded[j];
    }
  }

  std::string fullPath = FileSys::GetParentDirectory(strPath) + "/" + str.C_Str();
  std::shared_ptr texture = std::make_shared<Texture>();
  texture = Rain::ResourceManager::LoadTexture("diffuse", fullPath);
  texture->path = str.C_Str();

  std::cout << "Loading Texture: " << str.C_Str() << std::endl;

  textures_loaded.push_back(texture);

  return texture;
}

glm::mat4 convertToGLM(const aiMatrix4x4& from) {
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

MeshSource::MeshSource(std::string path) {
  Assimp::Importer import;
  const aiScene* scene = import.ReadFile(path,
                                         aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices);

  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
    std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
    return;
  }

  uint32_t offsetVertex = 0;
  uint32_t offsetIndex = 0;

  int verticesCount = 0;
  int indexCount = 0;

  for (int i = 0; i < scene->mNumMeshes; i++) {
    aiMesh* mesh = scene->mMeshes[i];
    verticesCount += mesh->mNumVertices;

    for (int j = 0; j < mesh->mNumFaces; j++) {
      indexCount += mesh->mFaces[j].mNumIndices;
    }
  }

  std::string fileName = FileSys::GetFileName(path);

  vertexBuffer = GPUAllocator::GAlloc("v_buffer_" + fileName, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex, (verticesCount * sizeof(VertexAttribute) + 3) & ~3);
  indexBuffer = GPUAllocator::GAlloc("i_buffer_" + fileName, WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index, (indexCount * sizeof(unsigned int) + 3) & ~3);

  aiColor3D colorEmpty = {0, 0, 0};
  m_Materials.resize(scene->mNumMaterials);

  for (int i = 0; i < scene->mNumMaterials; i++) {
    aiMaterial* mat = scene->mMaterials[i];

    aiColor3D color;
    float shininess;
    aiReturn result = AI_FAILURE;

    Ref<MaterialEngine> currentMaterial = CreateRef<MaterialEngine>();

    if (mat->Get(AI_MATKEY_COLOR_AMBIENT, mat) == AI_SUCCESS) {
      currentMaterial->properties.ambientColor = color != colorEmpty ? glm::vec3(color.r, color.g, color.b) : glm::vec3(0.1);
    }

    if (mat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
      currentMaterial->properties.diffuseColor = glm::vec3(color.r, color.g, color.b);
    }

    if (mat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
      currentMaterial->properties.specularColor = glm::vec3(color.r, color.g, color.b);
    }

    if (mat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
      currentMaterial->properties.shininess = (shininess / 10.0f) * 128.0f;
    }

    Ref<Texture> matTexture;
    aiString texturePath;

    for (int i = 0; i < mat->mNumProperties; i++) {
      auto mm = mat->mProperties[i];
      std::string decoded = std::string(mm->mData);
      printf("");
    }

    for (int j = 0; j < mat->GetTextureCount(aiTextureType_BASE_COLOR); j++) {
      if (mat->GetTexture(aiTextureType_DIFFUSE, j, &texturePath) != aiReturn_SUCCESS) {
        std::cout << "An error occured while loading texture" << std::endl;
        continue;
      }

      std::string textureName = FileSys::GetFileName(texturePath.C_Str());

      if (Rain::ResourceManager::IsTextureExist(textureName)) {
        matTexture = Rain::ResourceManager::GetTexture(textureName);
      } else {
        matTexture = Rain::ResourceManager::LoadTexture(textureName, texturePath.C_Str());
      }

      currentMaterial->SetDiffuseTexture("texture_diffuse" + std::to_string(j), matTexture);
    }

    MaterialEngine::CreateMaterial(currentMaterial);

    m_Materials[i] = currentMaterial;
  }

  m_SubMeshes.resize(scene->mNumMeshes);

  for (int i = 0; i < scene->mNumMeshes; i++) {
    aiMesh* mesh = scene->mMeshes[i];

    std::vector<VertexAttribute> vertices;
    std::vector<unsigned int> indices;

    for (int j = 0; j < mesh->mNumVertices; j++) {
      VertexAttribute vertex;
      glm::vec3 vector;
      vector.x = mesh->mVertices[j].x;
      vector.y = mesh->mVertices[j].y;
      vector.z = mesh->mVertices[j].z;
      vertex.Position = vector;

      if (mesh->HasNormals()) {
        vector.x = mesh->mNormals[j].x;
        vector.y = mesh->mNormals[j].y;
        vector.z = mesh->mNormals[j].z;
        vertex.Normal = vector;
      }

      if (mesh->HasTangentsAndBitangents()) {
        vector.x = mesh->mTangents[j].x;
        vector.y = mesh->mTangents[j].y;
        vector.z = mesh->mTangents[j].z;
        vertex.Tangent = vector;
      }

      if (mesh->mTextureCoords[0]) {
        glm::vec2 vec;
        vec.x = mesh->mTextureCoords[0][j].x;
        vec.y = mesh->mTextureCoords[0][j].y;
        vertex.TexCoords = vec;
      } else {
        vertex.TexCoords = glm::vec2(0.0f, 0.0f);
      }
      vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
      aiFace face = mesh->mFaces[i];
      for (unsigned int j = 0; j < face.mNumIndices; j++) {
        indices.push_back(face.mIndices[j]);
      }
    }

    uint32_t vertexBufferSize = vertices.size() * sizeof(VertexAttribute);
    uint32_t indexBufferSize = indices.size() * sizeof(unsigned int);

    wgpuQueueWriteBuffer(Render::Instance->m_queue,
                         vertexBuffer.Buffer,
                         offsetVertex * sizeof(VertexAttribute),
                         vertices.data(),
                         vertices.size() * sizeof(VertexAttribute));

    wgpuQueueWriteBuffer(Render::Instance->m_queue,
                         indexBuffer.Buffer,
                         offsetIndex * sizeof(unsigned int),
                         indices.data(),
                         indices.size() * sizeof(unsigned int));

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

  TraverseNode(scene->mRootNode, scene);
}

void MeshSource::TraverseNode(aiNode* node, const aiScene* scene) {
  if (node->mNumMeshes > 0) {
    Ref<MeshNode> meshNode = CreateRef<MeshNode>();
    meshNode->Name = std::string(node->mName.C_Str());
    meshNode->SubMeshId = node->mMeshes[0];  // For now just handle first mesh
    meshNode->LocalTransform = convertToGLM(node->mTransformation);

    m_SubMeshes[meshNode->SubMeshId].DrawCount++;
    m_Nodes.push_back(meshNode);
  }

  for (int i = 0; i < node->mNumChildren; i++) {
    TraverseNode(node->mChildren[i], scene);
  }
}
