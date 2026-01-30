#pragma once
#include <vector>

#include "imgui.h"
#include "ImGuizmo.h"
#include "engine/ImGuiLogSink.h"
#include "engine/Layer.h"
#include "scene/Scene.h"
#include "scene/SceneRenderer.h"

namespace Rain
{
  class EditorLayer : public Layer
  {
    virtual void OnAttach() override;
    virtual void OnDeattach() override;

    virtual void OnUpdate(float dt) override;
    virtual void OnRenderImGui() override;

    virtual void OnEvent(Event& event) override;

   private:
    void RenderLogViewer();
    void FilterLogs(const std::vector<LogEntry>& logs);

    void RenderEntityList();
    void RenderEntityNode(Entity entity);
    void RenderPropertyPanel();
    void RenderGizmo();

    Ref<Scene> m_Scene;
    Ref<SceneRenderer> m_ViewportRenderer;

    // Entity list state
    UUID m_SelectedEntityId = 0;

    // Log viewer state
    char m_SearchBuffer[256] = {0};
    std::vector<LogEntry> m_FilteredLogs;
    bool m_AutoScroll = true;
    bool m_ScrollToBottom = false;

    // Viewport settings
    bool m_ConstrainAspectRatio = true;
    float m_TargetAspectRatio = 16.0f / 9.0f;  // 1920x1080
    bool m_ViewportFocused = false;

    // Gizmo state
    ImGuizmo::OPERATION m_GizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE m_GizmoMode = ImGuizmo::WORLD;
    bool m_UseSnap = true;
    glm::vec3 m_SnapValue = {1.0f, 1.0f, 1.0f};

    // Viewport bounds for gizmo
    glm::vec2 m_ViewportBoundsMin = {0.0f, 0.0f};
    glm::vec2 m_ViewportBoundsMax = {0.0f, 0.0f};
  };
}  // namespace Rain
