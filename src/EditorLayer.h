#pragma once
#include <string>
#include <vector>

#include "imgui.h"
#include "ImGuizmo.h"
#include "engine/ImGuiLogSink.h"
#include "engine/Layer.h"
#include "map/MBTiles.h"
#include "scene/Scene.h"
#include "scene/SceneRenderer.h"

namespace WebEngine
{
  struct EditorCamera
  {
    glm::vec3 Position = {0.0f, 2.0f, 5.0f};
    float Yaw = -90.0f;  // Looking along -Z
    float Pitch = 0.0f;
    glm::vec3 Velocity = {0.0f, 0.0f, 0.0f};

    float MoveSpeed = 10.0f;
    float Acceleration = 30.0f;
    float Deceleration = 15.0f;
    float MouseSensitivity = 0.1f;

    glm::vec3 GetForward() const;
    glm::vec3 GetRight() const;
    glm::vec3 GetUp() const;
    glm::mat4 GetViewMatrix() const;
  };

  class EditorLayer : public Layer
  {
    virtual void OnAttach() override;
    virtual void OnDeattach() override;

    virtual void OnUpdate(float dt) override;
    virtual void OnRenderImGui() override;

    virtual void OnEvent(Event& event) override;

   private:
    void UpdateEditorCamera(float dt);
    void UpdateMapCamera(float dt);
    void ScanAvailableZooms();
    int SnapToAvailableZoom(int desiredZoom, int direction) const;
    void ComputeVisibleTileRect(int& minTX, int& minTY, int& maxTX, int& maxTY) const;
    void RefreshVisibleTiles(bool force = false);

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

    char m_SearchBuffer[256] = {0};
    std::vector<LogEntry> m_FilteredLogs;
    bool m_AutoScroll = true;
    bool m_ScrollToBottom = false;

    bool m_ConstrainAspectRatio = true;
    float m_TargetAspectRatio = 16.0f / 9.0f;  // 1920x1080
    bool m_ViewportFocused = false;

    ImGuizmo::OPERATION m_GizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE m_GizmoMode = ImGuizmo::WORLD;
    bool m_UseSnap = false;
    glm::vec3 m_SnapValue = {1.0f, 1.0f, 1.0f};

    glm::vec2 m_ViewportBoundsMin = {0.0f, 0.0f};
    glm::vec2 m_ViewportBoundsMax = {0.0f, 0.0f};

    EditorCamera m_EditorCamera;
    glm::vec2 m_LastMousePos = {0.0f, 0.0f};
    bool m_RightMouseDown = false;

    // Map mode
    bool m_MapMode = false;
    float m_MapWorldX = 0.0f;
    float m_MapWorldZ = 0.0f;
    float m_MapViewSize = 50.0f;
    int m_MapZoom = 12;
    int m_MapCenterTX = 2357;
    int m_MapCenterTY = 1573;
    std::string m_TileDbPath = "Resources/turkey.mbtiles";
    MBTilesReader m_TileSource;
    std::vector<int> m_AvailableZooms;  // sorted ascending; populated by ScanAvailableZooms

    // Cache of what's currently uploaded to the GPU so RefreshVisibleTiles can
    // skip no-op reloads. Sentinel: m_LoadedZoom == -1 means nothing loaded.
    int m_LoadedZoom = -1;
    int m_LoadedMinTX = 0;
    int m_LoadedMinTY = 0;
    int m_LoadedMaxTX = -1;
    int m_LoadedMaxTY = -1;
    int m_LoadedRefTX = 0;
    int m_LoadedRefTY = 0;
    bool m_MapDragging = false;
    glm::vec2 m_MapDragLastPos = {0.0f, 0.0f};
  };
}  // namespace WebEngine
