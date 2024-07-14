#pragma once
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <string>
#include "core/Ref.h"
#include "core/UUID.h"
#include "glm/ext/matrix_transform.hpp"
#include "render/Material.h"

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

  glm::mat4 GetTransform() const {
    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), Translation);
    glm::mat4 rotationMatrix = glm::eulerAngleYXZ(RotationEuler.y, RotationEuler.x, RotationEuler.z);
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), Scale);
    return translationMatrix * rotationMatrix * scaleMatrix;
  }

  void SetTransform(const glm::mat4& transform) {
    Translation = glm::vec3(transform[3]);

    Scale.x = glm::length(glm::vec3(transform[0]));
    Scale.y = glm::length(glm::vec3(transform[1]));
    Scale.z = glm::length(glm::vec3(transform[2]));

    glm::mat3 rotationMatrix = glm::mat3(
        glm::vec3(transform[0]) / Scale.x,
        glm::vec3(transform[1]) / Scale.y,
        glm::vec3(transform[2]) / Scale.z);

    RotationEuler = glm::eulerAngles(glm::quat_cast(rotationMatrix));
  }
};

struct RigidBodyComponent {
  float Mass = 1.0f;
  float LinearDrag = 0.01f;
  float AngularDrag = 0.05f;
  bool DisableGravity = false;

  glm::vec3 InitialLinearVelocity = glm::vec3(0.0f);
  glm::vec3 InitialAngularVelocity = glm::vec3(0.0f);

  RigidBodyComponent() = default;
  RigidBodyComponent(const RigidBodyComponent& other) = default;
};

struct RelationshipComponent {
  UUID ParentHandle = 0;
  std::vector<UUID> Children;

  RelationshipComponent() = default;
  RelationshipComponent(const RelationshipComponent& other) = default;
  RelationshipComponent(UUID parent)
      : ParentHandle(parent) {}
};

struct MeshComponent {
  Ref<MaterialTable> Materials = CreateRef<MaterialTable>();
  uint32_t MeshSourceId = -1;
  uint32_t SubMeshId = -1;

  MeshComponent(uint32_t meshSourceId = -1, uint32_t subMeshId = -1, uint32_t materialId = -1)
      : SubMeshId(subMeshId), MeshSourceId(meshSourceId) {}
};

struct DirectionalLightComponent {
  float Intensity = 0.0f;
};
