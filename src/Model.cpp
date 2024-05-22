#include "Model.h"
#include <iostream>
#include <ostream>
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

Model::Model(const char* path, MaterialUniform mat, WGPUSampler& textureSampler) {
  materialOverride = mat;
  loadModel(path, textureSampler);
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

  if (materialOverride.ambientColor == glm::vec3(0)) {
    aiReturn result = material->Get(AI_MATKEY_COLOR_AMBIENT, color);
    if (result == AI_SUCCESS) {
      materialUniform.ambientColor = color != colorEmpty ? glm::vec3(color.r, color.g, color.b) : glm::vec3(0.1);
    }
  } else {
    materialUniform.ambientColor = materialOverride.ambientColor;
  }

  if (materialOverride.diffuseColor == glm::vec3(0)) {
    result = material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
    if (result == AI_SUCCESS) {
      materialUniform.diffuseColor = glm::vec3(color.r, color.g, color.b);
    }
  } else {
    materialUniform.diffuseColor = materialOverride.diffuseColor;
  }

  if (materialOverride.specularColor == glm::vec3(0)) {
    result = material->Get(AI_MATKEY_COLOR_SPECULAR, color);
    if (result == AI_SUCCESS) {
      materialUniform.specularColor = glm::vec3(color.r, color.g, color.b);
    }
  } else {
    materialUniform.specularColor = materialOverride.specularColor;
  }

  if (materialOverride.shininess == -1) {
    result = material->Get(AI_MATKEY_SHININESS, shininess);
    if (result == AI_SUCCESS) {
      materialUniform.shininess = (shininess / 10.0f) * 128.0f;
    }
  } else {
    materialUniform.shininess = materialOverride.shininess;
  }

  auto m = CreateRef<Mesh>(vertices, indices, diffuseTexture, heightTexture, materialUniform, textureSampler);
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

void Model::Draw(WGPURenderPassEncoder& renderPass, WGPURenderPipeline& pipeline) {
  for (auto mesh : meshes) {
    mesh->Draw(renderPass, pipeline);
  }
}

void Model::UpdateUniforms(WGPUQueue& queue) {
  for (auto mesh : meshes) {
    mesh->UpdateUniforms(queue);
  }
}
