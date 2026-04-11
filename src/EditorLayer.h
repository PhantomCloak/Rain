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

  // Inclusive rectangle in tile coordinates.
  struct TileRect
  {
    int MinTX = 0;
    int MinTY = 0;
    int MaxTX = -1;
    int MaxTY = -1;

    bool operator==(const TileRect& o) const
    {
      return MinTX == o.MinTX && MinTY == o.MinTY && MaxTX == o.MaxTX && MaxTY == o.MaxTY;
    }
  };

  // Top-down orthographic camera for map mode. Position is expressed as a
  // reference tile + an offset in world units, which keeps float precision
  // stable as the camera pans across the globe.
  struct MapView
  {
    float WorldX = 0.0f;
    float WorldZ = 0.0f;
    float ViewSize = 50.0f;
    int Zoom = 12;
    int CenterTX = 2357;
    int CenterTY = 1573;

    // Camera position in fractional tile coordinates of the current zoom.
    glm::dvec2 CamTilePos() const;
    // Camera position as lat/lon (WGS84 degrees).
    glm::dvec2 CamLatLon() const;
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

    // Map mode update pipeline: each step reads + writes m_Map and leaves it
    // in a valid state for the next one.
    void UpdateMapCamera();
    void PanMapCamera(const glm::vec2& mousePos, bool leftMouseDown);
    void ZoomMapCamera(float scrollDelta);
    void RebaseMapOrigin();

    void ScanAvailableZooms();
    int SnapToAvailableZoom(int desiredZoom, int direction) const;
    TileRect ComputeVisibleTileRect() const;
    void RefreshVisibleTiles(bool force = false);
    SceneCamera BuildMapSceneCamera() const;

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
    bool m_MapMode = true;
    MapView m_Map;

    std::string m_TileDbPath = "Resources/turkey.mbtiles";
    MBTilesReader m_TileSource;
    std::vector<int> m_AvailableZooms;  // sorted ascending; populated by ScanAvailableZooms

    // Cache of what's currently uploaded to the GPU so RefreshVisibleTiles can
    // skip no-op reloads. Sentinel: m_LoadedZoom == -1 means nothing loaded.
    int m_LoadedZoom = -1;
    TileRect m_LoadedRect;
    int m_LoadedRefTX = 0;
    int m_LoadedRefTY = 0;

    bool m_MapDragging = false;
    glm::vec2 m_MapDragLastPos = {0.0f, 0.0f};
  };
}  // namespace WebEngine
