#pragma once
#include <memory>
#include "render/RenderMesh.h"

namespace Rain {
  struct MMesh {
    std::shared_ptr<RenderMesh> Tex;
    MMesh(std::shared_ptr<RenderMesh> tex) {
      Tex = tex;
    };
  };
}  // namespace Rain
