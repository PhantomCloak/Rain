#pragma once

#include <vector>
#include <glm/glm.hpp>

struct VertexAttributes {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
};

class Mesh {
public:
    std::vector<VertexAttributes> vertices;
    std::vector<unsigned int> indices;
public:
    Mesh(const std::vector<VertexAttributes>& vertices, const std::vector<unsigned int>& indices)
        : vertices(vertices), indices(indices) {
    }
};
