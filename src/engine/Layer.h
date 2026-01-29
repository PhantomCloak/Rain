#pragma once

#include "engine/Event.h"
namespace Rain
{
  class Layer
  {
   public:
    Layer() {};
    ~Layer() {};

    virtual void OnAttach() {};
    virtual void OnDeattach() {};

    virtual void OnUpdate(float dt) {};
    virtual void OnRenderImGui() {};

    virtual void OnEvent(Event& event) {};
  };
}  // namespace Rain
