#pragma once
#ifndef NDEBUG
#define NDEBUG
#endif

//#include <PxPhysicsAPI.h>
#include "Model.h"
#include "Node.h"
#include "webgpu/webgpu.h"

class GameObject : public Node {
	public:
		std::string name;
		GameObject(std::string objName, Ref<MeshSource> model);
		GameObject(std::string objName, Ref<Model> model);
		void Draw();
		void DrawAlt(WGPURenderPassEncoder& renderPass, WGPURenderPipeline& pipeline);
		void UpdateUniforms();
		void Update();
		bool IsNeedUpdate() const;
		void AddPhysics();
		void AddPhysicsSphere(glm::vec3 velocity);

		// Render Studd
		Ref<Model> model;
		Ref<MeshSource> modelExp;
		Ref<Material> materialOverride;

		// Physics Stuff
		//physx::PxRigidDynamic* pxActorDynamic;
		//physx::PxRigidStatic* pxActor;

		bool isStatic;
		WGPUBindGroup bgScene;
		std::vector<Ref<Mesh>> meshes;
		SceneUniform sceneUniform;
		WGPUBuffer sceneUniformBuffer;
};
