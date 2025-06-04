#include "RuntimeLayer.h"

namespace Rain {
  void RuntimeLayer::OnAttach() {
    m_Scene = std::make_unique<Scene>("Test Scene");
  }
}  // namespace Rain
