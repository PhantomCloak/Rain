//#ifndef __EMSCRIPTEN__
//#include "Physics.h"
//#include <iostream>
//
//Class RainPhysXErrorCallback : public PxErrorCallback {
// public:
//  virtual void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line) override {
//    std::cerr << "PhysX Error: " << message;
//    std::cerr << " (Error code: " << code << ", File: " << file
//              << ", Line: " << line << ")" << std::endl;
//
//		exit(0);
//  }
//};
//
//Physics* Physics::Instance = nullptr;
//
//Void Physics::Initialise() {
//	static RainPhysXErrorCallback errorCallback;
//
//  gFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, errorCallback);
//  gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation, PxTolerancesScale(), true);
//
//	gScene = CreateScene(glm::vec3(0, -9.7, 0));
//	Instance = this;
//}
//
//PxScene* Physics::CreateScene(glm::vec3 gravity) {
//  PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
//	sceneDesc.gravity = PxVec3(gravity.x, gravity.y, gravity.z);
//  sceneDesc.cpuDispatcher = PxDefaultCpuDispatcherCreate(1);
//  sceneDesc.filterShader = PxDefaultSimulationFilterShader;
//  return gPhysics->createScene(sceneDesc);
//}
//#endif
