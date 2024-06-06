#pragma once
#ifndef NDEBUG
#define NDEBUG
#endif

//#include <PxPhysicsAPI.h>
#include "Node.h"
#include "webgpu/webgpu.h"
#include "render/Model.h"

class GameObject : public Node {
	public:
		std::string name;
		GameObject(std::string objName, Ref<MeshSource> model);
		void Draw();
		void DrawAlt(WGPURenderPassEncoder& renderPass, WGPURenderPipeline& pipeline);
		void UpdateUniforms();
		void Update();
		bool IsNeedUpdate() const;
		void AddPhysics();
		void AddPhysicsSphere(glm::vec3 velocity);

		// Render Studd
		Ref<MeshSource> modelExp;

		// Physics Stuff
		//physx::PxRigidDynamic* pxActorDynamic;
		//physx::PxRigidStatic* pxActor;

		bool isStatic;
		WGPUBindGroup bgScene;
		SceneUniform sceneUniform;
		WGPUBuffer sceneUniformBuffer;
};
