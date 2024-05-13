#pragma once
#include <cstdint>

float quadVertices[] = {
    // positions        // texture Coords
    -1.0f,  1.0f, 0.0f, 0.0f, 0.0f,  // Top-Left Vertex     (V inverted)
    -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,  // Bottom-Left Vertex  (V inverted)
     1.0f,  1.0f, 0.0f, 1.0f, 0.0f,  // Top-Right Vertex    (V inverted)
     1.0f, -1.0f, 0.0f, 1.0f, 1.0f,  // Bottom-Right Vertex (V inverted)
};

uint32_t quadIndices[] = {
    0, 1, 2,  // First triangle
    2, 1, 3   // Second triangle
};
