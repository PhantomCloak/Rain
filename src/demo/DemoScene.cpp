#include "DemoScene.h"
#include "core/KeyCode.h"
#include "render/ResourceManager.h"
#include "io/keyboard.h"

namespace WebEngine
{
  void DemoSceneDefault::Init()
  {
    Scene::Init();

    const auto testModel = ResourceManager::LoadMeshSource("Resources/Kachujin/Kachujin.glb");
    const auto boxModel = ResourceManager::LoadMeshSource("Resources/box/box.gltf");
    const auto weaponModel = ResourceManager::LoadMeshSource("Resources/assault_rifle_pbr/assault_rifle_pbr.glb");

    Entity modelEntity = CreateEntity("character");
    Entity floorEntity = CreateEntity("floor");
    Entity weaponEntity = CreateEntity("weapon");

    modelEntity.Transform().Translation = glm::vec3(0, -0.0, 0);
    modelEntity.Transform().Scale = glm::vec3(0.04f);
    modelEntity.Transform().SetRotationEuler(glm::radians(glm::vec3(-90.0, 0.0, 180.0)));

    floorEntity.Transform().Translation = glm::vec3(0, -1.0, 0);
    floorEntity.Transform().Scale = glm::vec3(50.0f, 1.0f, 50.0f);

    weaponEntity.Transform().Translation = glm::vec3(17, 5.0, 0);
    weaponEntity.Transform().Scale = glm::vec3(3.0f);
    weaponEntity.Transform().SetRotationEuler(glm::radians(glm::vec3(-90.0, 0.0, 0.0)));

    BuildMeshEntityHierarchy(modelEntity, testModel);
    BuildMeshEntityHierarchy(floorEntity, boxModel);
    BuildMeshEntityHierarchy(weaponEntity, weaponModel);
  }

  void DemoScenePhysicCollisions::ScanKeyPress()
  {
    if (Keyboard::IsKeyPressed(Key::F))
    {
      ThrowBall(EditorCameraPosition, EditorCameraForward);
    }
  }

  void DemoScenePhysicCollisions::ThrowBall(const glm::vec3& position, const glm::vec3& direction)
  {
    float ballScale = 0.5f;
    float throwSpeed = 30.0f;

    std::string name = "ball_" + std::to_string(m_BallCounter++);
    Entity ball = CreateEntity(name);
    ball.Transform().Translation = position + direction * 2.0f;
    ball.Transform().Scale = glm::vec3(ballScale);
    BuildMeshEntityHierarchy(ball, m_BallMesh);

    auto& rb = ball.AddComponent<RigidBodyComponent>();
    rb.BodyType = EBodyType::Dynamic;
    rb.Mass = 5.0f;
    rb.InitialLinearVelocity = direction * throwSpeed;

    auto& collider = ball.AddComponent<BoxColliderComponent>();
    collider.Size = glm::vec3(ballScale);

    m_PhysicsScene->CreateBody(ball);
  }

  void DemoScenePhysicCollisions::Init()
  {
    Scene::Init();

    //const auto boxModel = ResourceManager::LoadMeshSource("Resources/box/box.gltf");
    //m_BallMesh = boxModel;

    //Entity floorEntity = CreateEntity("floor");
    //floorEntity.Transform().Translation = glm::vec3(0.0f, -0.5f, 0.0f);
    //floorEntity.Transform().Scale = glm::vec3(50.0f, 1.0f, 50.0f);
    //BuildMeshEntityHierarchy(floorEntity, boxModel);
    //auto& floorRb = floorEntity.AddComponent<RigidBodyComponent>();
    //floorRb.BodyType = EBodyType::Static;
    //auto& floorCollider = floorEntity.AddComponent<BoxColliderComponent>();
    //floorCollider.Size = glm::vec3(50.0f, 1.0f, 50.0f);
    //m_PhysicsScene->CreateBody(floorEntity);

    //float boxScale = 1.0f;
    //float boxActualSize = boxScale * 2.0f;
    //float startY = boxScale;
    //int layers = 5;

    //for (int layer = 0; layer < layers; layer++)
    //{
    //  int boxesInRow = layers - layer;
    //  float offsetX = -(boxesInRow - 1) * boxActualSize * 0.5f;
    //  float offsetZ = -(boxesInRow - 1) * boxActualSize * 0.5f;
    //  float y = startY + layer * boxActualSize;

    //  for (int x = 0; x < boxesInRow; x++)
    //  {
    //    for (int z = 0; z < boxesInRow; z++)
    //    {
    //      std::string name = "box_" + std::to_string(layer) + "_" + std::to_string(x) + "_" + std::to_string(z);
    //      Entity box = CreateEntity(name);
    //      box.Transform().Translation = glm::vec3(offsetX + x * boxActualSize, y, offsetZ + z * boxActualSize);
    //      BuildMeshEntityHierarchy(box, boxModel);

    //      auto& rb = box.AddComponent<RigidBodyComponent>();
    //      rb.BodyType = EBodyType::Dynamic;
    //      rb.Mass = 10.0f;
    //      auto& boxCollider = box.AddComponent<BoxColliderComponent>();
    //      boxCollider.Size = glm::vec3(boxScale);

    //      m_PhysicsScene->CreateBody(box);
    //    }
    //  }
    //}
  }

  void DemoSceneSponza::Init()
  {
    Scene::Init();
    const auto sponzaModel = ResourceManager::LoadMeshSource("Resources/Sponza/Sponza-KTX-Draco.glb");
    Entity preview = CreateEntity("Model");
    preview.Transform().Scale = glm::vec3(5.0f);
    BuildMeshEntityHierarchy(preview, sponzaModel);
  }

}  // namespace WebEngine
