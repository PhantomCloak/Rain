#include "EditorLayer.h"
#include "imgui.h"

namespace Rain
{
  void EditorLayer::OnAttach()
  {
    m_ViewportRenderer = CreateRef<SceneRenderer>();
    m_ViewportRenderer->Init();

    m_Scene = std::make_unique<Scene>("Test Scene");
    m_Scene->Init();
  }

  void EditorLayer::OnDeattach()
  {
  }

  void EditorLayer::OnUpdate(float dt)
  {
    m_Scene->OnUpdate();
    m_Scene->OnRender(m_ViewportRenderer);
  }

  void EditorLayer::OnRenderImGui()
  {
    ImGui::Begin("Viewport");

    auto texture = m_ViewportRenderer->GetLastPassImage();
    if (texture)
    {
      ImVec2 viewportSize = ImGui::GetContentRegionAvail();
      WGPUTextureView textureView = texture->GetReadableView(0);
      if (textureView)
      {
        ImGui::Image((ImTextureID)textureView, viewportSize);
      }
    }

    ImGui::End();
  }
}  // namespace Rain
