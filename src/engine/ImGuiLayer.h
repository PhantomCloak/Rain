#pragma once

#include "Layer.h"
#include "webgpu/webgpu.h"

namespace Rain
{
  class ImGuiLayer : public Layer
  {
   public:
    ImGuiLayer() = default;
    ~ImGuiLayer() = default;

    void OnAttach() override;
    void OnDeattach() override;

    void Begin();
    void End();

   private:
    WGPUTextureView m_CurrentTextureView = nullptr;
  };
}  // namespace Rain
