#pragma once
#include <memory>
#include "render/primitives/texture.h"

namespace Rain {
  struct MeshTexture {
    std::shared_ptr<Texture> Tex;
    MeshTexture(std::shared_ptr<Texture> tex) {
      Tex = tex;
    };
  };
}  // namespace Rain
