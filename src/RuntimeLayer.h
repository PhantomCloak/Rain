#pragma once
#include "engine/Layer.h"
#include "scene/Scene.h"

namespace Rain {
  class RuntimeLayer : public Layer {
    virtual void OnUpdate() override;
    virtual void OnRender() override;

    virtual void OnAttach() override;
    virtual void OnDeattach() override;

   private:
    std::unique_ptr<Scene> m_Scene;
  };
}  // namespace Rain
