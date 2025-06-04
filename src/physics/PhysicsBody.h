#pragma once
#include <Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include "scene/Entity.h"

namespace Rain {

  class PhysicsBody {
   public:
    PhysicsBody(JPH::BodyInterface& bodyInterface, Entity entity);
    JPH::BodyID GetBodyId() { return m_BodyID; }

    glm::vec3 GetTranslation() const;
    glm::quat GetRotation() const;
    bool IsSleeping() const;
    void SetSleepState(bool inSleep);
    void MoveKinematic(const glm::vec3& targetPosition, const glm::quat& targetRotation, float deltaSeconds);
    bool IsKinematic() const;
    void Update();

    void SetTranslation(const glm::vec3& translation);

    JPH::BodyID m_BodyID;

   private:
    void CreateStaticBody(JPH::BodyInterface& bodyInterface);
    void CreateDynamicBody(JPH::BodyInterface& bodyInterface);

    Entity m_Entity;
  };
}  // namespace Rain
