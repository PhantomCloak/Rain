#pragma once

#include <cstdint>

float quadVertices[] = {
	-1.0f,  1.0f, 0.0f,		0.0f,		0.0f, 0.0f,	0.0f,0.0f,
	-1.0f, -1.0f, 0.0f,		0.0f,		0.0f, 1.0f,	0.0f,0.0f,
	 1.0f,	1.0f,	0.0f, 	0.0f, 	1.0f, 0.0f,	0.0f,0.0f,
	 1.0f,	-1.0f,0.0f, 	0.0f, 	1.0f, 1.0f,	0.0f,0.0f,
};

uint32_t quadIndices[] = {
    0, 1, 2,  // First triangle
    2, 1, 3   // Second triangle
};
