#pragma once
#include "engine/Layer.h"
#include "scene/Scene.h"
#include "scene/SceneRenderer.h"

namespace Rain
{
  class EditorLayer : public Layer
  {
    virtual void OnAttach() override;
    virtual void OnDeattach() override;

    virtual void OnUpdate(float dt) override;
    virtual void OnRenderImGui() override;

    virtual void OnEvent(Event& event) override;

   private:
    Ref<Scene> m_Scene;
    Ref<SceneRenderer> m_ViewportRenderer;
  };
}  // namespace Rain
