#ifndef __EMSCRIPTEN__
#pragma once
#ifndef NDEBUG
#define NDEBUG
#endif

#include <PxPhysicsAPI.h>
#include <glm/glm.hpp>
#include <vector>

using namespace physx;

class PhysicUtils {
 public:
  static PxConvexMesh* createConvexMesh(const std::vector<glm::vec3>& vertices, unsigned int stride, PxPhysics* physics, PxCookingParams* cooking);
  static PxBoxGeometry* createBoxMesh(int hx, int hy, int z);
	static PxTransform ConvertPxTransform(glm::vec3 position, float rotation, glm::vec3 direction = glm::vec3(0, 1, 0));
	static PxTransform ConvertPxTransform(glm::vec3 position, glm::vec3 rotation);
};
#endif
