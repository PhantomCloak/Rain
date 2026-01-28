#pragma once

namespace Rain
{
  class Layer
  {
   public:
    Layer() {};
    ~Layer() {};

    virtual void OnUpdate(float dt) {};
    virtual void OnRenderImGui() {};

    virtual void OnAttach() {};
    virtual void OnDeattach() {};
  };
}  // namespace Rain
