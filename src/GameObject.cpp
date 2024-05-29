#pragma once
#include "GameObject.h"
#include "render/Render.h"
#include "render/RenderUtils.h"
#include "physics/Physics.h"
#include "physics/PhysicUtils.h"
#include "render/RenderQueue.h"
#include <PxPhysicsAPI.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>


GameObject::GameObject(std::string objName, Ref<MeshSource> model) {
  this->modelExp = model;

  if (Parent != nullptr) {
    sceneUniform.modelMatrix = Parent->GetModelMatrix() * GetModelMatrix();
  } else {
    sceneUniform.modelMatrix = GetModelMatrix();
  }
}

GameObject::GameObject(std::string objName, Ref<Model> model) {
  this->model = model;
  this->meshes = model->meshes;
	this->name = objName;

  if (Parent != nullptr) {
    sceneUniform.modelMatrix = Parent->GetModelMatrix() * GetModelMatrix();
  } else {
    sceneUniform.modelMatrix = GetModelMatrix();
  }
}

void GameObject::Draw() {
	RenderQueue::AddQueue(this);
}

void GameObject::DrawAlt(WGPURenderPassEncoder& renderPass, WGPURenderPipeline& pipeline) {
  wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);
  wgpuRenderPassEncoderSetBindGroup(renderPass, 0, bgScene, 0, NULL);

  //for (auto mesh : meshes) {
  //  mesh->Draw(renderPass, pipeline);
  //}
}

void GameObject::AddPhysics() {
	//return;
	static PxMaterial* material = Physics::Instance->gPhysics->createMaterial(0.8f, 0.8f, 0.1f);
	PxBoxGeometry cubeGeometry(PxVec3(1.0f, 1.0f, 1.0f));

	pxActorDynamic = PxCreateDynamic(*Physics::Instance->gPhysics,
			PhysicUtils::ConvertPxTransform(Transform.position, Transform.rotation),
			cubeGeometry,
			*material, 2.0f);

	PxTransform massOffset = PxTransform(PxVec3(0.0f, -0.5f, 0.0f));
	//pxActorDynamic->setCMassLocalPose(massOffset);
	//pxActorDynamic->setSolverIterationCounts(16, 4); // Default is 4 position, 1 velocity
	//pxActorDynamic->setLinearDamping(0.1f);
	//pxActorDynamic->setAngularDamping(0.1f);
	//pxActorDynamic->setSleepThreshold(0.05f);


	isStatic = false;
	Physics::Instance->gScene->addActor(*pxActorDynamic);
}


void GameObject::AddPhysicsSphere(glm::vec3 velocity) {
	//return;
  const float sphereRadius = 0.5f;
  const float sphereDensity = 10.0f;

	static PxMaterial* sphereMat = Physics::Instance->gPhysics->createMaterial(0.5f, 1.0f, 0.6f);

	PxSphereGeometry sphereGeometry(sphereRadius);

	PxTransform sphereTransform(PxVec3(Transform.position.x, Transform.position.y, Transform.position.z));
	pxActorDynamic = PxCreateDynamic(*Physics::Instance->gPhysics, sphereTransform, sphereGeometry, *sphereMat, sphereDensity);
  pxActorDynamic->setLinearVelocity(PxVec3(velocity.x, velocity.y, velocity.z));

	isStatic = false;
	Physics::Instance->gScene->addActor(*pxActorDynamic);
}

bool GameObject::IsNeedUpdate() const {
  if (isStatic) {
    return false;
  }

  if (pxActorDynamic == nullptr) {
    return false;
  }

  return pxActorDynamic->isSleeping();
}

void GameObject::Update() {
		//if(pxActorDynamic->isSleeping())
		//	return;

    PxTransform cubeTransform = pxActorDynamic->getGlobalPose();

    PxVec3 cubePos = cubeTransform.p;
    PxQuat cubeRot = cubeTransform.q;

    Transform.position = glm::vec3(cubePos.x, cubePos.y, cubePos.z);

    // Convert PxQuat to glm::quat
    glm::quat glmQuat(cubeRot.w, cubeRot.x, cubeRot.y, cubeRot.z);

    // Convert glm::quat to Euler angles
    glm::vec3 eulerAngles = glm::eulerAngles(glmQuat);

    // Convert Euler angles from radians to degrees
    Transform.rotation = glm::degrees(eulerAngles);

    UpdateUniforms();
}

void GameObject::UpdateUniforms() {
	return;
  sceneUniform.modelMatrix = GetModelMatrix();
  wgpuQueueWriteBuffer(Render::Instance->m_queue, sceneUniformBuffer, 0, &sceneUniform, sizeof(SceneUniform));
}
