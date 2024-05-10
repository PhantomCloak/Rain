#pragma once
#include <glm/glm.hpp>

struct RenderMeshUniform {
  glm::mat4x4 modelMatrix;
  glm::vec4 color;
};
