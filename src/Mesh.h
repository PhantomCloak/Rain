#pragma once
#include "Node.h"
#include "render/primitives/texture.h"
#include <vector>
#include <glm/glm.hpp>

struct VertexE {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    //glm::vec3 Tangent;
    //glm::vec3 BitTangent;
};

class MeshE : public Node {
    public:
        // mesh data
        std::vector<VertexE>       vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture>      textures;

        MeshE(std::vector<VertexE> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures);
        //void Draw(Shader &shader);
    private:
        //  render data
        unsigned int VAO, VBO, EBO;
};
