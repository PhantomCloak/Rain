#pragma once
#include <vector>
#include "Node.h"
#include "Mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class Model : public Node
{
    public:
        Model(const char *path)
        {
						Name = path;
            loadModel(path);
        }
        //void Draw(Shader &shader);	
        // model data
        std::vector<Ref<MeshE>> meshes;
        std::string directory;
				std::vector<std::shared_ptr<Texture>> textures_loaded;

        void loadModel(std::string path);
        void processNode(aiNode *node, const aiScene *scene);
        Ref<MeshE> processMesh(aiMesh *mesh, const aiScene *scene);
        std::vector<std::shared_ptr<Texture>> loadMaterialTextures(aiMaterial *mat, aiTextureType type, 
                                             std::string typeName);
			private:
				std::string strPath;
};

