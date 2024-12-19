//#ifndef __EMSCRIPTEN__
//
//#include "PhysicUtils.h"
//#include <iostream>
//#include <glm/gtc/quaternion.hpp>
//#include <glm/gtc/constants.hpp>
//
//PxConvexMesh* PhysicUtils::createConvexMesh(const std::vector<glm::vec3>& vertices, unsigned int stride, PxPhysics* physics, PxCookingParams* cooking) {
//  PxConvexMeshDesc convexDesc;
//  convexDesc.points.count = static_cast<PxU32>(vertices.size());
//  convexDesc.points.stride = stride;
//  convexDesc.points.data = vertices.data();
//  convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;
//
//  PxDefaultMemoryOutputStream buf;
//  //if (!cooking->cookConvexMesh(convexDesc, buf)) {
//  //  std::cout << "Failed to cook convex mesh." << std::endl;
//  //}
//
//  PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
//  return physics->createConvexMesh(input);
//}
//
//PxTransform PhysicUtils::ConvertPxTransform(glm::vec3 position, float rotation, glm::vec3 direction) {
//  float angle = glm::radians(rotation);
//	PxQuat rotationQ(angle, PxVec3(direction.x, direction.y, direction.z));
//
//	return PxTransform(PxVec3(position.x, position.y, position.z), rotationQ);
//}
//
//PxTransform PhysicUtils::ConvertPxTransform(glm::vec3 position, glm::vec3 rotation) {
//    glm::vec3 rotationRadians = glm::radians(rotation);
//    glm::quat glmQuat = glm::quat(rotationRadians);
//
//    PxQuat pxQuat(glmQuat.x, glmQuat.y, glmQuat.z, glmQuat.w);
//
//    return PxTransform(PxVec3(position.x, position.y, position.z), pxQuat);
//}
//#endif
