#pragma once

namespace Rain {
  class Actor {
   public:
    virtual void OnStart() = 0;
    virtual void PreUpdate() = 0;
    virtual void Update() = 0;
  };
}  // namespace Rain
