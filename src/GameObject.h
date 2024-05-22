#pragma once

#ifndef _DEBUG
#define _DEBUG
#endif

#include <PxPhysicsAPI.h>
#include "Model.h"
#include "Node.h"
#include "webgpu/webgpu.h"

class GameObject : public Node {
	public:
		GameObject(Ref<Model> model);
		void Draw(WGPURenderPassEncoder& renderPass, WGPURenderPipeline& pipeline);
		void UpdateUniforms();
		void Update();
		bool IsNeedUpdate() const;

		void AddPhysics();
		void AddPhysicsSphere(glm::vec3 velocity);
		physx::PxRigidDynamic* pxActorDynamic;
		physx::PxRigidStatic* pxActor;

		bool isStatic;
	private:
		WGPUBindGroup bgScene;
		Ref<Model> model;
		std::vector<Ref<Mesh>> meshes;
		SceneUniform sceneUniform;
		WGPUBuffer sceneUniformBuffer;
};
