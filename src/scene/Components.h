#pragma once
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <string>
#include "Math/Math.h"
#include "core/Ref.h"
#include "core/UUID.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"
#include "render/Material.h"
#include "render/Camera.h"

namespace Rain {
  struct IDComponent {
    UUID ID = 0;
  };

  struct TagComponent {
    std::string Tag;

    TagComponent() = default;
    TagComponent(const TagComponent& other) = default;
    TagComponent(const std::string& tag)
        : Tag(tag) {}

    operator std::string&() { return Tag; }
    operator const std::string&() const { return Tag; }
  };

  struct TransformComponent {
    glm::vec3 Translation = {0.0f, 0.0f, 0.0f};
    glm::vec3 Scale = {1.0f, 1.0f, 1.0f};
    glm::vec3 RotationEuler = {0.0f, 0.0f, 0.0f};
    glm::quat Rotation = {1.0f, 0.0f, 0.0f, 0.0f};

    glm::mat4 GetTransform() const {
      return glm::translate(glm::mat4(1.0f), Translation) * glm::toMat4(Rotation) * glm::scale(glm::mat4(1.0f), Scale);
    }

    void SetTransform(const glm::mat4& transform) {
      Rain::Math::DecomposeTransform(transform, Translation, Rotation, Scale);
      RotationEuler = glm::eulerAngles(Rotation);
    }

    void SetRotationEuler(const glm::vec3& euler) {
      RotationEuler = euler;
      Rotation = glm::quat(RotationEuler);
    }

    glm::quat GetRotation() const {
      return Rotation;
    }

    // from: Hazel Engine TransformComponent.SetRotation
    void SetRotation(const glm::quat& quat) {
      // wrap given euler angles to range [-pi, pi]
      auto wrapToPi = [](glm::vec3 v) {
        return glm::mod(v + glm::pi<float>(), 2.0f * glm::pi<float>()) - glm::pi<float>();
      };

      auto originalEuler = RotationEuler;
      Rotation = quat;
      RotationEuler = glm::eulerAngles(Rotation);

      // A given quat can be represented by many Euler angles (technically infinitely many),
      // and glm::eulerAngles() can only give us one of them which may or may not be the one we want.
      // Here we have a look at some likely alternatives and pick the one that is closest to the original Euler angles.
      // This is an attempt to avoid sudden 180deg flips in the Euler angles when we SetRotation(quat).

      glm::vec3 alternate1 = {RotationEuler.x - glm::pi<float>(), glm::pi<float>() - RotationEuler.y, RotationEuler.z - glm::pi<float>()};
      glm::vec3 alternate2 = {RotationEuler.x + glm::pi<float>(), glm::pi<float>() - RotationEuler.y, RotationEuler.z - glm::pi<float>()};
      glm::vec3 alternate3 = {RotationEuler.x + glm::pi<float>(), glm::pi<float>() - RotationEuler.y, RotationEuler.z + glm::pi<float>()};
      glm::vec3 alternate4 = {RotationEuler.x - glm::pi<float>(), glm::pi<float>() - RotationEuler.y, RotationEuler.z + glm::pi<float>()};

      // We pick the alternative that is closest to the original value.
      float distance0 = glm::length2(wrapToPi(RotationEuler - originalEuler));
      float distance1 = glm::length2(wrapToPi(alternate1 - originalEuler));
      float distance2 = glm::length2(wrapToPi(alternate2 - originalEuler));
      float distance3 = glm::length2(wrapToPi(alternate3 - originalEuler));
      float distance4 = glm::length2(wrapToPi(alternate4 - originalEuler));

      float best = distance0;
      if (distance1 < best) {
        best = distance1;
        RotationEuler = alternate1;
      }
      if (distance2 < best) {
        best = distance2;
        RotationEuler = alternate2;
      }
      if (distance3 < best) {
        best = distance3;
        RotationEuler = alternate3;
      }
      if (distance4 < best) {
        best = distance4;
        RotationEuler = alternate4;
      }

      RotationEuler = wrapToPi(RotationEuler);
    }
  };

  enum EBodyType {
    Dynamic,
    Static
  };

  struct BoxColliderComponent {
    glm::vec3 Offset = {0.0f, 0.0f, 0.0f};
    glm::vec3 Size = {0.5f, 0.5f, 0.5f};

    BoxColliderComponent() = default;
    BoxColliderComponent(const BoxColliderComponent& other) = default;
  };

  struct CylinderCollider {
    glm::vec2 Size = {0.5f, 0.5f};

    CylinderCollider() = default;
    CylinderCollider(const CylinderCollider& other) = default;
  };

  struct WheelColliderComponent {
    glm::vec3 LocalPosition;
    float Radius;
    float Width;
    float SuspensionMaxLen;
    float SuspensionMinLen;

    WheelColliderComponent() = default;
    WheelColliderComponent(const WheelColliderComponent& other) = default;
  };

  struct RigidBodyComponent {
    float Mass = 1.0f;
    float LinearDrag = 0.01f;
    float AngularDrag = 0.05f;
    bool DisableGravity = false;

    glm::vec3 InitialLinearVelocity = glm::vec3(0.0f);
    glm::vec3 InitialAngularVelocity = glm::vec3(0.0f);

    float MaxLinearVelocity = 500;
    float MaxAngularVelocity = 50;

    EBodyType BodyType = EBodyType::Static;

    RigidBodyComponent() = default;
    RigidBodyComponent(const RigidBodyComponent& other) = default;
  };

  struct TrackedVehicleComponent {
    glm::vec3 CenterOfMassOffset = glm::vec3(0.0f);
    float VehicleWidth = 3.4f;
    float FehicleLenght = 6.4f;
    float WheelRadius = 0.3f;
    float SuspensionMinLen = 0.3f;
    float SuspensionMaxLen = 0.5f;
    float WheelWidth = 1.7f;
    float MaxPitchRollAngle = 60.0f;
    float Mass = 4000.0f;
  };

  struct RelationshipComponent {
    UUID ParentHandle = 0;
    std::vector<UUID> Children;

    RelationshipComponent() = default;
    RelationshipComponent(const RelationshipComponent& other) = default;
    RelationshipComponent(UUID parent)
        : ParentHandle(parent) {}
  };

  struct CameraComponent {
    enum class Type { None = -1,
                      Perspective,
                      Orthographic };
    Type ProjectionType;

    Camera SceneCamera;
    bool Primary = true;

    CameraComponent() = default;
    CameraComponent(const CameraComponent& other) = default;

    operator Camera&() { return SceneCamera; }
    operator const Camera&() const { return SceneCamera; }
  };

  struct MeshComponent {
    Ref<MaterialTable> Materials = CreateRef<MaterialTable>();
    uint64_t MeshSourceId = -1;
    uint32_t SubMeshId = -1;

    MeshComponent(uint64_t meshSourceId = -1, uint32_t subMeshId = -1, uint32_t materialId = -1)
        : SubMeshId(subMeshId), MeshSourceId(meshSourceId) {}
  };

  struct DirectionalLightComponent {
    float Intensity = 0.0f;
  };
}  // namespace Rain
