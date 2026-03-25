#pragma once
#include "scene/Scene.h"

namespace WebEngine
{
  class DemoSceneDefault : public Scene
  {
   public:
    DemoSceneDefault(std::string sceneName = "Untitled Scene") : Scene(std::move(sceneName)) {}
    void Init() override;
  };

  class DemoScenePhysicCollisions : public Scene
  {
   public:
    DemoScenePhysicCollisions(std::string sceneName = "Untitled Scene") : Scene(std::move(sceneName)) {}
    void Init() override;
    void ScanKeyPress() override;

    void ThrowBall(const glm::vec3& position, const glm::vec3& direction);

   private:
    Ref<MeshSource> m_BallMesh;
    int m_BallCounter = 0;
  };
}  // namespace WebEngine
