#include "EditorLayer.h"
#include <algorithm>
#include <cstring>
#include <unordered_set>
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

    // Track viewport focus and handle Escape to unfocus
    m_ViewportFocused = ImGui::IsWindowFocused();
    if (m_ViewportFocused && ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
      ImGui::SetWindowFocus(nullptr);
      m_ViewportFocused = false;
    }

    auto texture = m_ViewportRenderer->GetLastPassImage();
    if (texture)
    {
      WGPUTextureView textureView = texture->GetReadableView(0);
      if (textureView)
      {
        ImVec2 area = ImGui::GetContentRegionAvail();

        if (m_ConstrainAspectRatio)
        {
          // Use 16:9 reference dimensions for aspect ratio calculation
          constexpr float srcWidth = 1920.0f;
          constexpr float srcHeight = 1080.0f;

          float scale = std::min(area.x / srcWidth, area.y / srcHeight);
          float sizeX = srcWidth * scale;
          float sizeY = srcHeight * scale;

          // Center the image
          ImVec2 cursorPos = ImGui::GetCursorPos();
          float posX = cursorPos.x + (area.x - sizeX) / 2.0f;
          float posY = cursorPos.y + (area.y - sizeY) / 2.0f;
          ImGui::SetCursorPos(ImVec2(posX, posY));

          ImGui::Image((ImTextureID)textureView, ImVec2(sizeX, sizeY));
        }
        else
        {
          ImGui::Image((ImTextureID)textureView, area);
        }
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

    // Build a set of entities that have parents (so we skip them at root level)
    std::unordered_set<UUID> childEntities;
    for (auto& entity : entities)
    {
      for (UUID childId : entity.Children())
      {
        childEntities.insert(childId);
      }
    }

    // Render only root entities (those without parents in the scene)
    for (auto& entity : entities)
    {
      UUID parentId = entity.GetParentUUID();
      if (parentId == 0 || childEntities.find(entity.GetUUID()) == childEntities.end())
      {
        // This is a root entity or its parent doesn't exist
        if (parentId == 0)
        {
          RenderEntityNode(entity);
        }
      }
    }

    // Click on empty space to deselect
    if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
    {
      m_SelectedEntity = {};
    }

    ImGui::End();
  }

  void EditorLayer::RenderEntityNode(Entity entity)
  {
    std::string name = entity.Name();
    auto& children = entity.Children();
    bool hasChildren = !children.empty();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (m_SelectedEntity == entity)
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
      m_SelectedEntity = entity;
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

    if (!m_SelectedEntity)
    {
      ImGui::TextDisabled("No entity selected");
      ImGui::End();
      return;
    }

    const ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

    // IDComponent
    if (m_SelectedEntity.HasComponent<IDComponent>())
    {
      if (ImGui::CollapsingHeader("ID", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##IDTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& id = m_SelectedEntity.GetComponent<IDComponent>();
          PropertyLabel("UUID");
          ImGui::Text("%llu", (unsigned long long)id.ID);

          ImGui::EndTable();
        }
      }
    }

    // TagComponent
    if (m_SelectedEntity.HasComponent<TagComponent>())
    {
      if (ImGui::CollapsingHeader("Tag", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##TagTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& tag = m_SelectedEntity.GetComponent<TagComponent>();
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
    if (m_SelectedEntity.HasComponent<TransformComponent>())
    {
      if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##TransformTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& transform = m_SelectedEntity.GetComponent<TransformComponent>();

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
    if (m_SelectedEntity.HasComponent<RelationshipComponent>())
    {
      if (ImGui::CollapsingHeader("Relationship"))
      {
        if (ImGui::BeginTable("##RelationshipTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& relationship = m_SelectedEntity.GetComponent<RelationshipComponent>();

          PropertyLabel("Parent UUID");
          ImGui::Text("%llu", (unsigned long long)relationship.ParentHandle);

          PropertyLabel("Children");
          ImGui::Text("%zu", relationship.Children.size());

          ImGui::EndTable();
        }
      }
    }

    // MeshComponent
    if (m_SelectedEntity.HasComponent<MeshComponent>())
    {
      if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##MeshTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& mesh = m_SelectedEntity.GetComponent<MeshComponent>();

          PropertyLabel("Mesh Source ID");
          ImGui::Text("%llu", (unsigned long long)mesh.MeshSourceId);

          PropertyLabel("SubMesh ID");
          ImGui::Text("%u", mesh.SubMeshId);

          ImGui::EndTable();
        }
      }
    }

    // CameraComponent
    if (m_SelectedEntity.HasComponent<CameraComponent>())
    {
      if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##CameraTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& camera = m_SelectedEntity.GetComponent<CameraComponent>();

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
    if (m_SelectedEntity.HasComponent<DirectionalLightComponent>())
    {
      if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##LightTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& light = m_SelectedEntity.GetComponent<DirectionalLightComponent>();

          PropertyLabel("Intensity");
          ImGui::DragFloat("##Intensity", &light.Intensity, 0.1f, 0.0f, 100.0f);

          ImGui::EndTable();
        }
      }
    }

    // RigidBodyComponent
    if (m_SelectedEntity.HasComponent<RigidBodyComponent>())
    {
      if (ImGui::CollapsingHeader("Rigid Body", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##RigidBodyTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& rb = m_SelectedEntity.GetComponent<RigidBodyComponent>();

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
    if (m_SelectedEntity.HasComponent<BoxColliderComponent>())
    {
      if (ImGui::CollapsingHeader("Box Collider", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##BoxColliderTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& collider = m_SelectedEntity.GetComponent<BoxColliderComponent>();

          PropertyLabel("Offset");
          ImGui::DragFloat3("##Offset", &collider.Offset.x, 0.1f);

          PropertyLabel("Size");
          ImGui::DragFloat3("##Size", &collider.Size.x, 0.1f, 0.0f, 100.0f);

          ImGui::EndTable();
        }
      }
    }

    // CylinderCollider
    if (m_SelectedEntity.HasComponent<CylinderCollider>())
    {
      if (ImGui::CollapsingHeader("Cylinder Collider", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##CylinderColliderTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& collider = m_SelectedEntity.GetComponent<CylinderCollider>();

          PropertyLabel("Size");
          ImGui::DragFloat2("##CylSize", &collider.Size.x, 0.1f, 0.0f, 100.0f);

          ImGui::EndTable();
        }
      }
    }

    // TrackedVehicleComponent
    if (m_SelectedEntity.HasComponent<TrackedVehicleComponent>())
    {
      if (ImGui::CollapsingHeader("Tracked Vehicle", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##TrackedVehicleTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& vehicle = m_SelectedEntity.GetComponent<TrackedVehicleComponent>();

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
    if (m_SelectedEntity.HasComponent<AnimatorComponent>())
    {
      if (ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (ImGui::BeginTable("##AnimatorTable", 2, tableFlags))
        {
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

          auto& animator = m_SelectedEntity.GetComponent<AnimatorComponent>();

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

  void EditorLayer::OnEvent(Event& event)
  {
    // Only pass events to scene when viewport is focused
    if (!m_ViewportFocused)
      return;

    if (event.GetEventType() == EventType::MouseMove)
    {
      MouseMoveEvent& e = static_cast<MouseMoveEvent&>(event);
      glm::vec2 p = e.GetPosition();
      m_Scene->OnMouseMove(p.x, p.y);
    }
  }
}  // namespace Rain
