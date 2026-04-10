#include "EditorLayer.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_set>
#include "demo/DemoScene.h"
#include "glm/fwd.hpp"
#include "imgui.h"
#include "ImGuizmo.h"
#include "Application.h"
#include "render/ResourceManager.h"
#include "map/MapProjection.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace WebEngine
{
  glm::vec3 EditorCamera::GetForward() const
  {
    float yawRad = glm::radians(Yaw);
    float pitchRad = glm::radians(Pitch);
    return glm::normalize(glm::vec3(
        std::cos(yawRad) * std::cos(pitchRad),
        std::sin(pitchRad),
        std::sin(yawRad) * std::cos(pitchRad)));
  }

  glm::vec3 EditorCamera::GetRight() const
  {
    return glm::normalize(glm::cross(GetForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
  }

  glm::vec3 EditorCamera::GetUp() const
  {
    return glm::normalize(glm::cross(GetRight(), GetForward()));
  }

  glm::mat4 EditorCamera::GetViewMatrix() const
  {
    return glm::lookAt(Position, Position + GetForward(), glm::vec3(0.0f, 1.0f, 0.0f));
  }

  static bool RayIntersectsOBB(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::mat4& modelMatrix, const glm::vec3& aabbMin, const glm::vec3& aabbMax, float& outDistance)
  {
    glm::mat4 invModel = glm::inverse(modelMatrix);
    glm::vec3 localOrigin = glm::vec3(invModel * glm::vec4(rayOrigin, 1.0f));
    glm::vec3 localDir = glm::vec3(invModel * glm::vec4(rayDir, 0.0f));

    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();

    for (int i = 0; i < 3; i++)
    {
      if (std::abs(localDir[i]) < 1e-8f)
      {
        if (localOrigin[i] < aabbMin[i] || localOrigin[i] > aabbMax[i])
        {
          return false;
        }
      }
      else
      {
        float invD = 1.0f / localDir[i];
        float t1 = (aabbMin[i] - localOrigin[i]) * invD;
        float t2 = (aabbMax[i] - localOrigin[i]) * invD;

        if (t1 > t2)
        {
          std::swap(t1, t2);
        }
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);

        if (tMin > tMax)
        {
          return false;
        }
      }
    }

    glm::vec3 localHit = localOrigin + localDir * tMin;
    glm::vec3 worldHit = glm::vec3(modelMatrix * glm::vec4(localHit, 1.0f));
    outDistance = glm::distance(rayOrigin, worldHit);
    return true;
  }

  void EditorLayer::OnAttach()
  {
    m_ViewportRenderer = CreateRef<SceneRenderer>();
    m_ViewportRenderer->Init();

    // m_Scene = std::make_unique<DemoSceneDefault>("Test Scene");
    m_Scene = std::make_unique<DemoScenePhysicCollisions>("Test Scene");
    m_Scene->Init();

    m_EditorCamera.Position = glm::vec3(-10, 10.5, -24.1f);
    m_EditorCamera.Yaw = 48.29;
    m_EditorCamera.Pitch = -14.90;

    // Open the .mbtiles archive and query its zoom pyramid. All subsequent
    // tile loads go through this reader.
    if (!m_TileSource.Open(m_TileDbPath))
    {
      RN_LOG_ERR("EditorLayer: failed to open tile database '{}'", m_TileDbPath);
    }
    ScanAvailableZooms();
    if (!m_AvailableZooms.empty())
    {
      m_MapZoom = SnapToAvailableZoom(m_MapZoom, 0);
    }
    RefreshVisibleTiles(true);
  }

  void EditorLayer::ScanAvailableZooms()
  {
    m_AvailableZooms = m_TileSource.GetAvailableZooms();
    if (m_AvailableZooms.empty())
    {
      RN_LOG_ERR("EditorLayer: No zoom levels found in '{}'", m_TileDbPath);
    }
    else
    {
      RN_LOG("EditorLayer: Available zoom levels in '{}': {} ({}..{})",
             m_TileDbPath, m_AvailableZooms.size(),
             m_AvailableZooms.front(), m_AvailableZooms.back());
    }
  }

  int EditorLayer::SnapToAvailableZoom(int desiredZoom, int direction) const
  {
    if (m_AvailableZooms.empty())
      return desiredZoom;

    auto it = std::lower_bound(m_AvailableZooms.begin(), m_AvailableZooms.end(), desiredZoom);
    if (it != m_AvailableZooms.end() && *it == desiredZoom)
      return desiredZoom;

    if (direction > 0)
    {
      // Prefer the next level at or above the request.
      if (it != m_AvailableZooms.end())
        return *it;
      return m_AvailableZooms.back();
    }
    if (direction < 0)
    {
      // Prefer the next level at or below the request.
      if (it != m_AvailableZooms.begin())
        return *(it - 1);
      return m_AvailableZooms.front();
    }
    // Nearest
    if (it == m_AvailableZooms.begin())
      return *it;
    if (it == m_AvailableZooms.end())
      return m_AvailableZooms.back();
    int higher = *it;
    int lower = *(it - 1);
    return (desiredZoom - lower <= higher - desiredZoom) ? lower : higher;
  }

  void EditorLayer::ComputeVisibleTileRect(int& minTX, int& minTY, int& maxTX, int& maxTY) const
  {
    // Ortho camera footprint in world units. The map projection flips the
    // X axis on screen but the visible X range is still symmetric around the
    // camera, so we don't need to mirror here.
    const float aspect = 16.0f / 9.0f;
    const float halfW = m_MapViewSize * 0.5f;
    const float halfH = halfW / aspect;

    const float tileSize = MapProjection::TILE_WORLD_SIZE;

    // Camera position in absolute fractional tile space. (+0.5 because tile
    // (T,T) is centered at world origin when ref = T.)
    const double camTileX = (double)m_MapCenterTX + 0.5 + (double)m_MapWorldX / tileSize;
    const double camTileY = (double)m_MapCenterTY + 0.5 - (double)m_MapWorldZ / tileSize;

    const double halfTilesW = (double)halfW / tileSize;
    const double halfTilesH = (double)halfH / tileSize;

    // One-tile margin so tiles entering the viewport are already uploaded by
    // the time they become visible.
    minTX = (int)std::floor(camTileX - halfTilesW) - 1;
    maxTX = (int)std::floor(camTileX + halfTilesW) + 1;
    minTY = (int)std::floor(camTileY - halfTilesH) - 1;
    maxTY = (int)std::floor(camTileY + halfTilesH) + 1;

    // Clamp to valid tile range for this zoom.
    const int maxIndex = (1 << m_MapZoom) - 1;
    minTX = glm::clamp(minTX, 0, maxIndex);
    maxTX = glm::clamp(maxTX, 0, maxIndex);
    minTY = glm::clamp(minTY, 0, maxIndex);
    maxTY = glm::clamp(maxTY, 0, maxIndex);
  }

  void EditorLayer::RefreshVisibleTiles(bool force)
  {
    if (!m_TileSource.IsOpen())
      return;

    int minTX, minTY, maxTX, maxTY;
    ComputeVisibleTileRect(minTX, minTY, maxTX, maxTY);

    if (!force &&
        m_LoadedZoom == m_MapZoom &&
        m_LoadedMinTX == minTX && m_LoadedMinTY == minTY &&
        m_LoadedMaxTX == maxTX && m_LoadedMaxTY == maxTY &&
        m_LoadedRefTX == m_MapCenterTX && m_LoadedRefTY == m_MapCenterTY)
    {
      return;
    }

    m_ViewportRenderer->ReloadMapTiles(m_TileSource, m_MapZoom,
                                       minTX, minTY, maxTX, maxTY,
                                       m_MapCenterTX, m_MapCenterTY);

    m_LoadedZoom = m_MapZoom;
    m_LoadedMinTX = minTX;
    m_LoadedMinTY = minTY;
    m_LoadedMaxTX = maxTX;
    m_LoadedMaxTY = maxTY;
    m_LoadedRefTX = m_MapCenterTX;
    m_LoadedRefTY = m_MapCenterTY;
  }

  void EditorLayer::OnDeattach()
  {
  }

  void EditorLayer::UpdateEditorCamera(float dt)
  {
    bool rightMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);

    if (rightMouseDown && m_ViewportFocused)
    {
      ImVec2 mousePos = ImGui::GetMousePos();
      if (m_RightMouseDown)
      {
        float deltaX = mousePos.x - m_LastMousePos.x;
        float deltaY = mousePos.y - m_LastMousePos.y;

        m_EditorCamera.Yaw += deltaX * m_EditorCamera.MouseSensitivity;
        m_EditorCamera.Pitch -= deltaY * m_EditorCamera.MouseSensitivity;
        m_EditorCamera.Pitch = glm::clamp(m_EditorCamera.Pitch, -89.0f, 89.0f);
      }
      m_LastMousePos = {mousePos.x, mousePos.y};
    }
    m_RightMouseDown = rightMouseDown;

    glm::vec3 inputDir(0.0f);
    if (rightMouseDown && m_ViewportFocused)
    {
      if (ImGui::IsKeyDown(ImGuiKey_W))
      {
        inputDir += m_EditorCamera.GetForward();
      }
      if (ImGui::IsKeyDown(ImGuiKey_S))
      {
        inputDir -= m_EditorCamera.GetForward();
      }
      if (ImGui::IsKeyDown(ImGuiKey_D))
      {
        inputDir += m_EditorCamera.GetRight();
      }
      if (ImGui::IsKeyDown(ImGuiKey_A))
      {
        inputDir -= m_EditorCamera.GetRight();
      }
    }

    if (glm::length(inputDir) > 0.001f)
    {
      inputDir = glm::normalize(inputDir);
      glm::vec3 targetVelocity = inputDir * (m_EditorCamera.MoveSpeed * (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? 4.0f : 1.0f));
      m_EditorCamera.Velocity = glm::mix(m_EditorCamera.Velocity, targetVelocity,
                                         1.0f - std::exp(-m_EditorCamera.Acceleration * dt));
    }
    else
    {
      m_EditorCamera.Velocity = glm::mix(m_EditorCamera.Velocity, glm::vec3(0.0f),
                                         1.0f - std::exp(-m_EditorCamera.Deceleration * dt));
    }

    m_EditorCamera.Position += m_EditorCamera.Velocity * dt;
  }

  void EditorLayer::UpdateMapCamera(float dt)
  {
    ImVec2 mousePos = ImGui::GetMousePos();

    // Pan with left mouse button drag
    bool leftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if (leftDown && m_ViewportFocused)
    {
      if (m_MapDragging)
      {
        float dx = mousePos.x - m_MapDragLastPos.x;
        float dy = mousePos.y - m_MapDragLastPos.y;

        // Convert screen pixels to world units
        float viewportW = m_ViewportBoundsMax.x - m_ViewportBoundsMin.x;
        if (viewportW > 1.0f)
        {
          float pixelsToWorld = m_MapViewSize / viewportW;
          m_MapWorldX -= dx * pixelsToWorld;
          m_MapWorldZ += dy * pixelsToWorld;
        }
      }
      m_MapDragLastPos = {mousePos.x, mousePos.y};
      m_MapDragging = true;
    }
    else
    {
      m_MapDragging = false;
    }

    // Zoom: scroll scales the orthographic view size for continuous (smooth)
    // zoom, and only swaps to a different tile LOD when the view drifts outside
    // a comfort range. Missing zoom levels are skipped via SnapToAvailableZoom.
    if (m_ViewportFocused)
    {
      float scroll = ImGui::GetIO().MouseWheel;
      if (scroll != 0.0f)
      {
        // Continuous-tile-space cursor: where the camera currently points, in
        // fractional tile coordinates of the CURRENT zoom. Using this avoids
        // the equirectangular drift of the previous lat/lon approximation when
        // we cross zoom levels.
        double camTileX = (double)m_MapCenterTX + 0.5 + (double)m_MapWorldX / MapProjection::TILE_WORLD_SIZE;
        double camTileY = (double)m_MapCenterTY + 0.5 - (double)m_MapWorldZ / MapProjection::TILE_WORLD_SIZE;
        double n = (double)(1 << m_MapZoom);
        double camLon = camTileX / n * 360.0 - 180.0;
        double camLatRad = std::atan(std::sinh(glm::pi<double>() * (1.0 - 2.0 * camTileY / n)));
        double camLat = camLatRad * 180.0 / glm::pi<double>();

        // Smooth view-size zoom. Scroll up (toward the user) shrinks the view
        // size → camera sees less world → visual zoom-in.
        constexpr float kZoomStep = 0.8f;
        float mult = (scroll > 0.0f) ? kZoomStep : (1.0f / kZoomStep);
        m_MapViewSize *= mult;
        m_MapViewSize = glm::clamp(m_MapViewSize, 0.1f, 10000.0f);

        // Comfort range for the current LOD: each tile is TILE_WORLD_SIZE wide,
        // so a comfortable view shows ~2..12 tiles before we want a new LOD.
        const float minView = MapProjection::TILE_WORLD_SIZE * 2.0f;
        const float maxView = MapProjection::TILE_WORLD_SIZE * 12.0f;

        int desiredZoom = m_MapZoom;
        if (m_MapViewSize < minView)
          desiredZoom = m_MapZoom + 1;  // want finer tiles
        else if (m_MapViewSize > maxView)
          desiredZoom = m_MapZoom - 1;  // want coarser tiles

        int targetZoom = SnapToAvailableZoom(desiredZoom, scroll > 0.0f ? +1 : -1);

        if (targetZoom != m_MapZoom)
        {
          int dz = targetZoom - m_MapZoom;
          // Preserve visual scale across the LOD switch: at zoom Z+1 the same
          // physical distance spans 2x as many world units, so V_new = V_old * 2^dz.
          m_MapViewSize *= std::pow(2.0f, (float)dz);
          // Across a multi-level gap the physical-scale rescale can leave the
          // view outside the comfort range; clamp so the result is always
          // something reasonable to look at.
          m_MapViewSize = glm::clamp(m_MapViewSize, minView, maxView);

          m_MapZoom = targetZoom;

          glm::ivec2 newCenter = MapProjection::LatLonToTile(camLat, camLon, m_MapZoom);
          m_MapCenterTX = newCenter.x;
          m_MapCenterTY = newCenter.y;
          m_MapWorldX = 0.0f;
          m_MapWorldZ = 0.0f;
        }
      }
    }

    // Rebase the world origin to the tile under the camera whenever it drifts
    // more than a few tiles away. This keeps m_MapWorldX/Z bounded for float
    // precision; RefreshVisibleTiles picks up the ref change and reloads.
    float distFromCenter = std::sqrt(m_MapWorldX * m_MapWorldX + m_MapWorldZ * m_MapWorldZ);
    if (distFromCenter > MapProjection::TILE_WORLD_SIZE * 3.0f)
    {
      glm::ivec2 tileOff = MapProjection::WorldToTileOffset(m_MapWorldX, m_MapWorldZ);

      m_MapWorldX -= (float)tileOff.x * MapProjection::TILE_WORLD_SIZE;
      m_MapWorldZ += (float)tileOff.y * MapProjection::TILE_WORLD_SIZE;

      m_MapCenterTX += tileOff.x;
      m_MapCenterTY += tileOff.y;
    }

    // Single source of truth for what's on the GPU: compute the visible rect
    // from the current camera state and reload only if it actually changed.
    RefreshVisibleTiles();
  }

  void EditorLayer::OnUpdate(float dt)
  {
    if (m_MapMode)
    {
      UpdateMapCamera(dt);

      m_ViewportRenderer->SetScene(m_Scene.get());

      // Orthographic top-down camera with equirectangular projection
      float aspect = 16.0f / 9.0f;
      float halfW = m_MapViewSize * 0.5f;
      float halfH = halfW / aspect;

      // Build orthographic projection with [0,1] depth range (WebGPU).
      // Flip left/right so screen-right = east (+X), screen-up = north (+Z).
      float left = halfW, right = -halfW, bottom = -halfH, top = halfH;
      float nearZ = 0.1f, farZ = 500.0f;
      glm::mat4 proj(0.0f);
      proj[0][0] = 2.0f / (right - left);
      proj[1][1] = 2.0f / (top - bottom);
      proj[2][2] = -1.0f / (farZ - nearZ);
      proj[3][0] = -(right + left) / (right - left);
      proj[3][1] = -(top + bottom) / (top - bottom);
      proj[3][2] = -nearZ / (farZ - nearZ);
      proj[3][3] = 1.0f;

      glm::vec3 eye(m_MapWorldX, 200.0f, m_MapWorldZ);
      glm::vec3 target(m_MapWorldX, 0.0f, m_MapWorldZ);
      glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0.0f, 0.0f, 1.0f));

      m_ViewportRenderer->BeginScene({view, proj, 0.1f, 500.0f});
      m_ViewportRenderer->EndScene();
    }
    else
    {
      UpdateEditorCamera(dt);
      m_Scene->EditorCameraPosition = m_EditorCamera.Position;
      m_Scene->EditorCameraForward = m_EditorCamera.GetForward();
      m_Scene->OnUpdate();
      m_Scene->OnRender(m_ViewportRenderer, m_EditorCamera.GetViewMatrix());
    }
  }

  void EditorLayer::OnRenderImGui()
  {
    ImGuizmo::BeginFrame();

    if (ImGui::BeginMainMenuBar())
    {
      if (ImGui::BeginMenu("File"))
      {
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Entity"))
      {
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Demo"))
      {
        if (ImGui::MenuItem("Stacking Boxes"))
        {
          m_Scene->Cleanup();
          m_Scene.reset();

          m_Scene = std::make_unique<DemoScenePhysicCollisions>("Test Scene");
          m_Scene->Init();
        }
        if (ImGui::MenuItem("Animations & PBR"))
        {
          m_Scene->Cleanup();
          m_Scene.reset();

          m_Scene = std::make_unique<DemoSceneDefault>("Test Scene");
          m_Scene->Init();
        }
        if (ImGui::MenuItem("Sponza"))
        {
          m_Scene->Cleanup();
          m_Scene.reset();

          m_Scene = std::make_unique<DemoSceneSponza>("Test Scene");
          m_Scene->Init();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Map View", nullptr, m_MapMode))
        {
          m_MapMode = !m_MapMode;
          if (m_MapMode)
          {
            // Reset map camera to tile grid center
            m_MapWorldX = 0.0f;
            m_MapWorldZ = 0.0f;
            m_MapViewSize = 50.0f;
          }
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    ImGui::Begin("Viewport");

    m_ViewportFocused = ImGui::IsWindowFocused();
    if (m_ViewportFocused && ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
      ImGui::SetWindowFocus(nullptr);
      m_ViewportFocused = false;
    }

    bool rightMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    if (!m_MapMode && m_ViewportFocused && !ImGuizmo::IsUsing() && !rightMouseDown)
    {
      if (ImGui::IsKeyPressed(ImGuiKey_T))
      {
        m_GizmoOperation = ImGuizmo::TRANSLATE;
      }
      if (ImGui::IsKeyPressed(ImGuiKey_R))
      {
        m_GizmoOperation = ImGuizmo::ROTATE;
      }
      if (ImGui::IsKeyPressed(ImGuiKey_E))
      {
        m_GizmoOperation = ImGuizmo::SCALE;
      }
      if (ImGui::IsKeyPressed(ImGuiKey_Q))
      {
        m_GizmoMode = (m_GizmoMode == ImGuizmo::LOCAL) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
      }
    }

    auto texture = m_ViewportRenderer->GetLastPassImage();
    if (texture)
    {
      WGPUTextureView textureView = texture->GetReadView(0);
      if (textureView)
      {
        ImVec2 area = ImGui::GetContentRegionAvail();
        ImVec2 imagePos;
        ImVec2 imageSize;

        if (m_ConstrainAspectRatio)
        {
          constexpr float srcWidth = 1920.0f;
          constexpr float srcHeight = 1080.0f;

          float scale = std::min(area.x / srcWidth, area.y / srcHeight);
          float sizeX = srcWidth * scale;
          float sizeY = srcHeight * scale;

          ImVec2 cursorPos = ImGui::GetCursorPos();
          float posX = cursorPos.x + (area.x - sizeX) / 2.0f;
          float posY = cursorPos.y + (area.y - sizeY) / 2.0f;
          ImGui::SetCursorPos(ImVec2(posX, posY));

          ImVec2 windowPos = ImGui::GetWindowPos();
          imagePos = ImVec2(windowPos.x + posX, windowPos.y + posY);
          imageSize = ImVec2(sizeX, sizeY);

          ImGui::Image((ImTextureID)textureView, ImVec2(sizeX, sizeY));
        }
        else
        {
          ImVec2 windowPos = ImGui::GetWindowPos();
          ImVec2 cursorPos = ImGui::GetCursorPos();
          imagePos = ImVec2(windowPos.x + cursorPos.x, windowPos.y + cursorPos.y);
          imageSize = area;

          ImGui::Image((ImTextureID)textureView, area);
        }

        m_ViewportBoundsMin = {imagePos.x, imagePos.y};
        m_ViewportBoundsMax = {imagePos.x + imageSize.x, imagePos.y + imageSize.y};

        if (!m_MapMode)
          RenderGizmo();

        // Viewport click-to-select via CPU ray-OBB picking
        if (!m_MapMode && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGuizmo::IsOver() && !rightMouseDown)
        {
          ImVec2 mousePos = ImGui::GetMousePos();
          float mx = mousePos.x - m_ViewportBoundsMin.x;
          float my = mousePos.y - m_ViewportBoundsMin.y;
          float viewportW = m_ViewportBoundsMax.x - m_ViewportBoundsMin.x;
          float viewportH = m_ViewportBoundsMax.y - m_ViewportBoundsMin.y;

          if (mx >= 0 && my >= 0 && mx < viewportW && my < viewportH)
          {
            float ndcX = (2.0f * mx / viewportW) - 1.0f;
            float ndcY = 1.0f - (2.0f * my / viewportH);

            glm::vec2 windowSize = Application::Get()->GetWindowSize();
            glm::mat4 projection = glm::perspectiveFov(glm::radians(55.0f), windowSize.x, windowSize.y, 0.1f, 1400.0f);
            glm::mat4 view = m_EditorCamera.GetViewMatrix();

            glm::vec4 clipPos(ndcX, ndcY, -1.0f, 1.0f);
            glm::vec4 eyePos = glm::inverse(projection) * clipPos;
            eyePos = glm::vec4(eyePos.x, eyePos.y, -1.0f, 0.0f);
            glm::vec3 rayDir = glm::normalize(glm::vec3(glm::inverse(view) * eyePos));
            glm::vec3 rayOrigin = m_EditorCamera.Position;

            float closestDist = std::numeric_limits<float>::max();
            UUID closestEntity = 0;

            auto meshEntities = m_Scene->GetAllEntitiesWithComponent<MeshComponent>();
            // RN_LOG("Viewport click: ray=({:.2f},{:.2f},{:.2f})->({:.2f},{:.2f},{:.2f}), entities={}",
            //        rayOrigin.x, rayOrigin.y, rayOrigin.z, rayDir.x, rayDir.y, rayDir.z, meshEntities.size());

            for (const auto& entity : meshEntities)
            {
              glm::mat4 worldTransform = m_Scene->GetWorldSpaceTransformMatrix(entity);

              auto& meshComp = entity.GetComponent<MeshComponent>();
              Ref<MeshSource> meshSource = ResourceManager::GetMeshSource(meshComp.MeshSourceId);
              if (!meshSource || meshComp.SubMeshId >= meshSource->m_SubMeshes.size())
              {
                continue;
              }

              const SubMesh& subMesh = meshSource->m_SubMeshes[meshComp.SubMeshId];

              float scaleX = glm::length(glm::vec3(worldTransform[0]));
              float scaleY = glm::length(glm::vec3(worldTransform[1]));
              float scaleZ = glm::length(glm::vec3(worldTransform[2]));
              float minScale = glm::max(glm::min(scaleX, glm::min(scaleY, scaleZ)), 0.001f);
              float localMinExtent = 3.0f / minScale;

              glm::vec3 center = (subMesh.BoundsMin + subMesh.BoundsMax) * 0.5f;
              glm::vec3 halfExt = glm::max((subMesh.BoundsMax - subMesh.BoundsMin) * 0.5f, glm::vec3(localMinExtent));

              glm::mat4 invModel = glm::inverse(worldTransform);
              glm::vec3 localOrig = glm::vec3(invModel * glm::vec4(rayOrigin, 1.0f));
              glm::vec3 localDir = glm::vec3(invModel * glm::vec4(rayDir, 0.0f));
              glm::vec3 worldPos = glm::vec3(worldTransform[3]);

              float dist;
              bool hit = RayIntersectsOBB(rayOrigin, rayDir, worldTransform, center - halfExt, center + halfExt, dist);
              // RN_LOG("  '{}' worldPos=({:.2f},{:.2f},{:.2f}) localRay=({:.1f},{:.1f},{:.1f})->({:.4f},{:.4f},{:.4f}) aabb=({:.1f},{:.1f},{:.1f})-({:.1f},{:.1f},{:.1f}) hit={}",
              //        entity.Name(),
              //        worldPos.x, worldPos.y, worldPos.z,
              //        localOrig.x, localOrig.y, localOrig.z,
              //        localDir.x, localDir.y, localDir.z,
              //        (center - halfExt).x, (center - halfExt).y, (center - halfExt).z,
              //        (center + halfExt).x, (center + halfExt).y, (center + halfExt).z, hit);
              if (hit && dist < closestDist)
              {
                closestDist = dist;
                closestEntity = entity.GetUUID();
              }
            }

            if (closestEntity != 0)
            {
              Entity hitEntity = m_Scene->TryGetEntityWithUUID(closestEntity);
              while (hitEntity && hitEntity.GetParentUUID() != 0)
              {
                Entity parent = m_Scene->TryGetEntityWithUUID(hitEntity.GetParentUUID());
                if (!parent)
                {
                  break;
                }
                hitEntity = parent;
              }
              closestEntity = hitEntity.GetUUID();
            }
            m_SelectedEntityId = closestEntity;
          }
        }
      }
    }

    ImGui::End();

    if (m_MapMode)
    {
      // Map info overlay
      ImGui::Begin("Map Info");
      glm::dvec2 centerLatLon = MapProjection::TileCenterToLatLon(m_MapCenterTX, m_MapCenterTY, m_MapZoom);
      ImGui::Text("Lat/Lon: %.4f, %.4f", centerLatLon.x, centerLatLon.y);
      ImGui::Text("Zoom: %d  Tile: %d/%d", m_MapZoom, m_MapCenterTX, m_MapCenterTY);
      ImGui::Separator();
      ImGui::Text("Pan: left-click drag");
      ImGui::Text("Zoom: scroll wheel (changes tile zoom %d..14)", 0);
      ImGui::End();
    }
    else
    {
      RenderEntityList();
      RenderPropertyPanel();
    }
    RenderLogViewer();
  }

  void EditorLayer::RenderEntityList()
  {
    ImGui::Begin("Entity List");

    auto entities = m_Scene->GetAllEntitiesWithComponent<TransformComponent>();

    std::unordered_set<UUID> childEntities;
    for (auto& entity : entities)
    {
      for (const UUID& childId : entity.Children())
      {
        childEntities.insert(childId);
      }
    }

    for (auto& entity : entities)
    {
      UUID parentId = entity.GetParentUUID();
      if (parentId == 0 || childEntities.find(entity.GetUUID()) == childEntities.end())
      {
        if (parentId == 0)
        {
          RenderEntityNode(entity);
        }
      }
    }

    if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
    {
      m_SelectedEntityId = 0;
    }

    ImGui::End();
  }

  void EditorLayer::RenderEntityNode(Entity entity)
  {
    std::string name = entity.Name();
    auto& children = entity.Children();
    bool hasChildren = !children.empty();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (m_SelectedEntityId == entity.GetUUID())
    {
      flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (!hasChildren)
    {
      flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entity.GetUUID(), flags, "%s", name.c_str());

    if (ImGui::IsItemClicked())
    {
      m_SelectedEntityId = entity.GetUUID();
    }

    if (hasChildren && opened)
    {
      for (const UUID& childId : children)
      {
        Entity childEntity = m_Scene->TryGetEntityWithUUID(childId);
        if (childEntity)
        {
          RenderEntityNode(childEntity);
        }
      }
      ImGui::TreePop();
    }
  }

  static void PropertyLabel(const char* label)
  {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
  }

  void EditorLayer::RenderPropertyPanel()
  {
    ImGui::Begin("Properties");

    Entity selectedEntity = m_Scene->TryGetEntityWithUUID(m_SelectedEntityId);
    if (!selectedEntity)
    {
      ImGui::TextDisabled("No entity selected");
      ImGui::End();
      return;
    }

    const ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

    // IDComponent
    if (selectedEntity.HasComponent<IDComponent>())
    {
      if (ImGui::CollapsingHeader("ID", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##IDTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& id = selectedEntity.GetComponent<IDComponent>();
          PropertyLabel("UUID");
          ImGui::Text("%llu", (unsigned long long)id.ID);

          ImGui::EndTable();
        }
      }
    }

    // TagComponent
    if (selectedEntity.HasComponent<TagComponent>())
    {
      if (ImGui::CollapsingHeader("Tag", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##TagTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& tag = selectedEntity.GetComponent<TagComponent>();
          char buffer[256];
          strncpy(buffer, tag.Tag.c_str(), sizeof(buffer) - 1);
          buffer[sizeof(buffer) - 1] = '\0';

          PropertyLabel("Name");
          if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
          {
            tag.Tag = std::string(buffer);
          }

          ImGui::EndTable();
        }
      }
    }

    // TransformComponent
    if (selectedEntity.HasComponent<TransformComponent>())
    {
      if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##TransformTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& transform = selectedEntity.GetComponent<TransformComponent>();

          PropertyLabel("Position");
          ImGui::DragFloat3("##Position", &transform.Translation.x, 0.1f);

          glm::vec3 rotationDegrees = glm::degrees(transform.RotationEuler);
          PropertyLabel("Rotation");
          if (ImGui::DragFloat3("##Rotation", &rotationDegrees.x, 1.0f))
          {
            transform.SetRotationEuler(glm::radians(rotationDegrees));
          }

          PropertyLabel("Scale");
          ImGui::DragFloat3("##Scale", &transform.Scale.x, 0.1f);

          ImGui::EndTable();
        }
      }
    }

    // RelationshipComponent
    if (selectedEntity.HasComponent<RelationshipComponent>())
    {
      if (ImGui::CollapsingHeader("Relationship"))
      {
        if (ImGui::BeginTable("##RelationshipTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& relationship = selectedEntity.GetComponent<RelationshipComponent>();

          PropertyLabel("Parent UUID");
          ImGui::Text("%llu", (unsigned long long)relationship.ParentHandle);

          PropertyLabel("Children");
          ImGui::Text("%zu", relationship.Children.size());

          ImGui::EndTable();
        }
      }
    }

    // MeshComponent
    if (selectedEntity.HasComponent<MeshComponent>())
    {
      if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##MeshTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& mesh = selectedEntity.GetComponent<MeshComponent>();

          PropertyLabel("Mesh Source ID");
          ImGui::Text("%llu", (unsigned long long)mesh.MeshSourceId);

          PropertyLabel("SubMesh ID");
          ImGui::Text("%u", mesh.SubMeshId);

          ImGui::EndTable();
        }
      }
    }

    // CameraComponent
    if (selectedEntity.HasComponent<CameraComponent>())
    {
      if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##CameraTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& camera = selectedEntity.GetComponent<CameraComponent>();

          PropertyLabel("Primary");
          ImGui::Checkbox("##Primary", &camera.Primary);

          const char* projTypes[] = {"None", "Perspective", "Orthographic"};
          int currentType = static_cast<int>(camera.ProjectionType) + 1;
          PropertyLabel("Projection");
          if (ImGui::Combo("##Projection", &currentType, projTypes, 3))
          {
            camera.ProjectionType = static_cast<CameraComponent::Type>(currentType - 1);
          }

          ImGui::EndTable();
        }
      }
    }

    // DirectionalLightComponent
    if (selectedEntity.HasComponent<DirectionalLightComponent>())
    {
      if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##LightTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& light = selectedEntity.GetComponent<DirectionalLightComponent>();

          PropertyLabel("Intensity");
          ImGui::DragFloat("##Intensity", &light.Intensity, 0.1f, 0.0f, 100.0f);

          ImGui::EndTable();
        }
      }
    }

    // RigidBodyComponent
    if (selectedEntity.HasComponent<RigidBodyComponent>())
    {
      if (ImGui::CollapsingHeader("Rigid Body", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##RigidBodyTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& rb = selectedEntity.GetComponent<RigidBodyComponent>();

          PropertyLabel("Mass");
          ImGui::DragFloat("##Mass", &rb.Mass, 0.1f, 0.0f, 10000.0f);

          PropertyLabel("Linear Drag");
          ImGui::DragFloat("##LinearDrag", &rb.LinearDrag, 0.01f, 0.0f, 1.0f);

          PropertyLabel("Angular Drag");
          ImGui::DragFloat("##AngularDrag", &rb.AngularDrag, 0.01f, 0.0f, 1.0f);

          PropertyLabel("Disable Gravity");
          ImGui::Checkbox("##DisableGravity", &rb.DisableGravity);

          const char* bodyTypes[] = {"Dynamic", "Static"};
          int currentBodyType = static_cast<int>(rb.BodyType);
          PropertyLabel("Body Type");
          if (ImGui::Combo("##BodyType", &currentBodyType, bodyTypes, 2))
          {
            rb.BodyType = static_cast<EBodyType>(currentBodyType);
          }

          PropertyLabel("Init Linear Vel");
          ImGui::DragFloat3("##InitLinearVel", &rb.InitialLinearVelocity.x, 0.1f);

          PropertyLabel("Init Angular Vel");
          ImGui::DragFloat3("##InitAngularVel", &rb.InitialAngularVelocity.x, 0.1f);

          PropertyLabel("Max Linear Vel");
          ImGui::DragFloat("##MaxLinearVel", &rb.MaxLinearVelocity, 1.0f, 0.0f, 1000.0f);

          PropertyLabel("Max Angular Vel");
          ImGui::DragFloat("##MaxAngularVel", &rb.MaxAngularVelocity, 1.0f, 0.0f, 100.0f);

          ImGui::EndTable();
        }
      }
    }

    // BoxColliderComponent
    if (selectedEntity.HasComponent<BoxColliderComponent>())
    {
      if (ImGui::CollapsingHeader("Box Collider", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##BoxColliderTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& collider = selectedEntity.GetComponent<BoxColliderComponent>();

          PropertyLabel("Offset");
          ImGui::DragFloat3("##Offset", &collider.Offset.x, 0.1f);

          PropertyLabel("Size");
          ImGui::DragFloat3("##Size", &collider.Size.x, 0.1f, 0.0f, 100.0f);

          ImGui::EndTable();
        }
      }
    }

    // CylinderCollider
    if (selectedEntity.HasComponent<CylinderCollider>())
    {
      if (ImGui::CollapsingHeader("Cylinder Collider", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##CylinderColliderTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& collider = selectedEntity.GetComponent<CylinderCollider>();

          PropertyLabel("Size");
          ImGui::DragFloat2("##CylSize", &collider.Size.x, 0.1f, 0.0f, 100.0f);

          ImGui::EndTable();
        }
      }
    }

    // TrackedVehicleComponent
    if (selectedEntity.HasComponent<TrackedVehicleComponent>())
    {
      if (ImGui::CollapsingHeader("Tracked Vehicle", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##TrackedVehicleTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& vehicle = selectedEntity.GetComponent<TrackedVehicleComponent>();

          PropertyLabel("COM Offset");
          ImGui::DragFloat3("##COMOffset", &vehicle.CenterOfMassOffset.x, 0.1f);

          PropertyLabel("Vehicle Width");
          ImGui::DragFloat("##VehicleWidth", &vehicle.VehicleWidth, 0.1f, 0.0f, 20.0f);

          PropertyLabel("Vehicle Length");
          ImGui::DragFloat("##VehicleLength", &vehicle.FehicleLenght, 0.1f, 0.0f, 20.0f);

          PropertyLabel("Wheel Radius");
          ImGui::DragFloat("##WheelRadius", &vehicle.WheelRadius, 0.01f, 0.0f, 2.0f);

          PropertyLabel("Suspension Min");
          ImGui::DragFloat("##SuspensionMin", &vehicle.SuspensionMinLen, 0.01f, 0.0f, 2.0f);

          PropertyLabel("Suspension Max");
          ImGui::DragFloat("##SuspensionMax", &vehicle.SuspensionMaxLen, 0.01f, 0.0f, 2.0f);

          PropertyLabel("Wheel Width");
          ImGui::DragFloat("##WheelWidth", &vehicle.WheelWidth, 0.1f, 0.0f, 5.0f);

          PropertyLabel("Max Pitch/Roll");
          ImGui::DragFloat("##MaxPitchRoll", &vehicle.MaxPitchRollAngle, 1.0f, 0.0f, 90.0f);

          PropertyLabel("Mass");
          ImGui::DragFloat("##VehicleMass", &vehicle.Mass, 10.0f, 0.0f, 100000.0f);

          ImGui::EndTable();
        }
      }
    }

    // AnimatorComponent
    if (selectedEntity.HasComponent<AnimatorComponent>())
    {
      if (ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##AnimatorTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& animator = selectedEntity.GetComponent<AnimatorComponent>();

          PropertyLabel("Playing");
          ImGui::Checkbox("##Playing", &animator.Playing);

          ImGui::EndTable();
        }
      }
    }

    ImGui::End();
  }

  void EditorLayer::FilterLogs(const std::vector<LogEntry>& logs)
  {
    m_FilteredLogs.clear();

    if (m_SearchBuffer[0] == '\0')
    {
      m_FilteredLogs = logs;
      return;
    }

    std::string searchStr(m_SearchBuffer);
    for (const auto& entry : logs)
    {
      if (entry.message.find(searchStr) != std::string::npos)
      {
        m_FilteredLogs.push_back(entry);
      }
    }
  }

  void EditorLayer::RenderLogViewer()
  {
    if (!ImGui::Begin("Log Viewer"))
    {
      ImGui::End();
      return;
    }

    if (ImGui::Button("Clear"))
    {
      ImGuiLogSink::Get().Clear();
    }
    ImGui::SameLine();

    ImGui::Checkbox("Auto-scroll", &m_AutoScroll);
    ImGui::SameLine();

    ImGui::Text("Search:");
    ImGui::SameLine();
    if (ImGui::InputText("##SearchField", m_SearchBuffer, sizeof(m_SearchBuffer)))
    {
      m_ScrollToBottom = true;
    }

    ImGui::Separator();

    auto logs = ImGuiLogSink::Get().CopyLogs();
    FilterLogs(logs);

    ImGui::BeginChild("LogRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& entry : m_FilteredLogs)
    {
      ImVec4 color;
      switch (entry.level)
      {
        case LogLevel::Trace:
          color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);  // Gray
          break;
        case LogLevel::Debug:
          color = ImVec4(0.6f, 0.6f, 0.9f, 1.0f);  // Light blue
          break;
        case LogLevel::Info:
          color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  // Green
          break;
        case LogLevel::Warn:
          color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow
          break;
        case LogLevel::Error:
          color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Red
          break;
        case LogLevel::Critical:
          color = ImVec4(1.0f, 0.0f, 0.5f, 1.0f);  // Magenta
          break;
        default:
          color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // White
          break;
      }
      ImGui::TextColored(color, "%s", entry.message.c_str());
    }

    if (m_ScrollToBottom || (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
    {
      ImGui::SetScrollHereY(1.0f);
      m_ScrollToBottom = false;
    }

    ImGui::EndChild();

    ImGui::End();
  }

  void EditorLayer::RenderGizmo()
  {
    Entity selectedEntity = m_Scene->TryGetEntityWithUUID(m_SelectedEntityId);
    if (!selectedEntity || !selectedEntity.HasComponent<TransformComponent>())
    {
      return;
    }

    glm::mat4 view = m_EditorCamera.GetViewMatrix();
    glm::vec2 windowSize = Application::Get()->GetWindowSize();
    glm::mat4 projection = glm::perspectiveFov(glm::radians(55.0f), windowSize.x, windowSize.y, 0.1f, 1400.0f);

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(m_ViewportBoundsMin.x, m_ViewportBoundsMin.y,
                      m_ViewportBoundsMax.x - m_ViewportBoundsMin.x,
                      m_ViewportBoundsMax.y - m_ViewportBoundsMin.y);

    glm::mat4 worldTransform = m_Scene->GetWorldSpaceTransformMatrix(selectedEntity);

    glm::vec3 snap = (m_GizmoOperation == ImGuizmo::ROTATE) ? glm::vec3(45.0f) : m_SnapValue;

    bool manipulated = ImGuizmo::Manipulate(
        glm::value_ptr(view), glm::value_ptr(projection),
        m_GizmoOperation, m_GizmoMode, glm::value_ptr(worldTransform),
        nullptr, m_UseSnap ? glm::value_ptr(snap) : nullptr);

    if (!manipulated)
    {
      return;
    }

    glm::mat4 localTransform = worldTransform;
    if (UUID parentId = selectedEntity.GetParentUUID(); parentId != 0)
    {
      if (Entity parent = m_Scene->TryGetEntityWithUUID(parentId))
      {
        localTransform = glm::inverse(m_Scene->GetWorldSpaceTransformMatrix(parent)) * worldTransform;
      }
    }

    selectedEntity.Transform().SetTransform(localTransform);
  }

  void EditorLayer::OnEvent(Event& event)
  {
  }
}  // namespace WebEngine
