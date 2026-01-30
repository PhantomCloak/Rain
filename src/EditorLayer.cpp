#include "EditorLayer.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_set>
#include "imgui.h"
#include "ImGuizmo.h"
#include "Application.h"
#include <glm/gtc/type_ptr.hpp>

namespace Rain
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

  void EditorLayer::OnUpdate(float dt)
  {
    UpdateEditorCamera(dt);
    m_Scene->OnUpdate();
    m_Scene->OnRender(m_ViewportRenderer, m_EditorCamera.GetViewMatrix());
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
    if (m_ViewportFocused && !ImGuizmo::IsUsing() && !rightMouseDown)
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
      WGPUTextureView textureView = texture->GetReadableView(0);
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

        RenderGizmo();
      }
    }

    ImGui::End();

    RenderEntityList();
    RenderPropertyPanel();
    RenderLogViewer();
  }

  void EditorLayer::RenderEntityList()
  {
    ImGui::Begin("Entity List");

    auto entities = m_Scene->GetAllEntitiesWithComponent<TransformComponent>();

    std::unordered_set<UUID> childEntities;
    for (auto& entity : entities)
    {
      for (UUID childId : entity.Children())
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
      for (UUID childId : children)
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
}  // namespace Rain
