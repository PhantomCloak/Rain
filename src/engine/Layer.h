#pragma once

namespace Rain {
  class Layer {
   public:
    Layer();

    virtual void OnUpdate() = 0;
    virtual void OnRender() = 0;

    virtual void OnAttach() = 0;
    virtual void OnDeattach() = 0;
  };
}  // namespace Rain
