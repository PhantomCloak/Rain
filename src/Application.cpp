#include "Application.h"
#include <GLFW/glfw3.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include "Game.h"
#include "Model.h"
#include "render/Camera.h"
#include "render/Render.h"
#include "render/RenderMesh.h"
#include "render/ResourceManager.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include "imgui.h"
#include "scene/Scene.h"

#include <iostream>
#if __EMSCRIPTEN__
#include <emscripten.h>
#include "platform/web/web_window.h"
#endif
// #ifndef _DEBUG
#define _DEBUG

extern "C" WGPUSurface glfwGetWGPUSurface(WGPUInstance instance, GLFWwindow* window);

glm::vec3 cameraFront = glm::vec3(0.0f, -0.5f, 0);

float screenWidth = 1920;
float screenHeight = 1080;

bool switchKey = false;
GLFWwindow* m_window;
Render* render = new Render();
Camera* Cam;

std::unique_ptr<PipelineManager> m_PipelineManager;
std::shared_ptr<ShaderManager> m_ShaderManager;
WGPURenderPipeline m_pipeline = nullptr;
WGPURenderPipeline m_pipeline_lit = nullptr;
std::unordered_map<int, bool> keyStates;

std::vector<RenderMesh*> renderMeshes;

void SetupScene() {
  glm::mat4 projectionMatrix = glm::perspective(90 * PI / 180, screenWidth / screenHeight, 0.01f, 1000.0f);
  glm::mat4 viewMatrix = glm::lookAt(glm::vec3(-2.0f, -3.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0, 0, 1));

  Cam = new Camera(render->m_device, m_pipeline, projectionMatrix, viewMatrix);
  Cam->updateBuffer(render->m_queue);
  auto boxMesh = Rain::ResourceManager::GetMesh("M_Box");
  auto boxTexture = Rain::ResourceManager::GetTexture("T_Box");

  auto planeMesh = Rain::ResourceManager::GetMesh("M_Floor");
  auto planeTexture = Rain::ResourceManager::GetTexture("T_Floor");

  auto meshBox = new RenderMesh(render->m_device, m_pipeline, boxTexture->Texture, boxTexture->View, render->m_sampler);

  Model* m = new Model(RESOURCE_DIR "/wood.obj");
  std::vector<VertexE> v = m->meshes[0]->vertices;

  //meshBox->SetVertexBuffer(render->m_device, render->m_queue, boxMesh->vertices);
  meshBox->SetVertexBuffer2(render->m_device, render->m_queue, v);

  meshBox->uniform.color = {0.0f, 1.0f, 0.4f, 1.0f};


  meshBox->uniform.modelMatrix = glm::mat4x4(1);
  meshBox->name = "box";

  meshBox->uniform.modelMatrix = glm::translate(meshBox->uniform.modelMatrix, glm::vec3(0, 0, 10));
  meshBox->uniform.modelMatrix = glm::scale(meshBox->uniform.modelMatrix, glm::vec3(1));

  //meshFloor->updateBuffer(render->m_queue);
  meshBox->updateBuffer(render->m_queue);

  renderMeshes.push_back(meshBox);
  //renderMeshes.push_back(meshFloor);
}

WGPUSurface m_surface;
void Application::OnStart() {
  WGPUInstance instance = render->CreateInstance();
  m_window = static_cast<GLFWwindow*>(GetNativeWindow());

#if __EMSCRIPTEN__
  m_surface = htmlGetCanvasSurface(instance, "canvas");
#else
  m_surface = glfwGetWGPUSurface(instance, m_window);
#endif

  if (m_surface == nullptr) {
    std::cout << "An error occured surface is null" << std::endl;
  }

  render->Init(m_window, instance, m_surface);
  m_ShaderManager = std::make_shared<ShaderManager>(render->m_device);

  auto devicePtr = std::make_shared<WGPUDevice>(render->m_device);
  Rain::ResourceManager::Init(devicePtr);

  Rain::ResourceManager::LoadMesh("M_Floor", RESOURCE_DIR "/floor.obj");
  Rain::ResourceManager::LoadTexture("T_Floor", RESOURCE_DIR "/floor.png");
  //Model* m = new Model(RESOURCE_DIR "/wood.obj");

  Rain::ResourceManager::LoadMesh("M_Box", RESOURCE_DIR "/wood.obj");
  Rain::ResourceManager::LoadTexture("T_Box", RESOURCE_DIR "/wood.png");

  m_ShaderManager->LoadShader("SH_Default", RESOURCE_DIR "/shader_default.wgsl");

  m_PipelineManager = std::make_unique<PipelineManager>(render->m_device, m_ShaderManager);

  BufferLayout vertexLayout = {
      {ShaderDataType::Float3, "position"},
      {ShaderDataType::Float3, "normal"},
      {ShaderDataType::Float2, "uv"}};

  GroupLayout groupLayout = {
      {0, GroupLayoutVisibility::Both, GroupLayoutType::Default},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler},
      {1, GroupLayoutVisibility::Both, GroupLayoutType::Default}};

  m_pipeline = m_PipelineManager->CreatePipeline("RP_Default", "SH_Default", vertexLayout, groupLayout, m_surface, render->m_adapter);

  render->SetClearColor(0.52, 0.80, 0.92, 1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO();

  ImGui_ImplGlfw_InitForOther(m_window, true);

	ImGui_ImplWGPU_InitInfo initInfo;
	initInfo.Device = render->m_device;
	initInfo.RenderTargetFormat = render->m_swapChainFormat;
	initInfo.DepthStencilFormat = render->m_depthTextureFormat;

  ImGui_ImplWGPU_Init(&initInfo);

  SetupScene();
}

bool Application::isRunning() {
  return !glfwWindowShouldClose(m_window);
}

void Application::OnResize(int height, int width) {
  render->screenHeight = height;
  render->screenWidth = width;
  render->m_swapChainDesc.height = height;
  render->m_swapChainDesc.width = width;

  render->m_swapChain = render->buildSwapChain(render->m_swapChainDesc, render->m_device, m_surface);
  render->m_depthTexture = render->GetDepthBufferTexture(render->m_device, render->m_swapChainDesc);
  render->m_depthTextureView =
      render->GetDepthBufferTextureView(render->m_depthTexture, render->m_depthTextureFormat);
  float ratio = render->m_swapChainDesc.width / (float)render->m_swapChainDesc.height;

  Cam->uniform.projectionMatrix =
      glm::perspective(90 * PI / 180, ratio, 0.01f, 100.0f);
  Cam->updateBuffer(render->m_queue);
}

bool firstMouse = true;
float lastX = screenWidth / 2.0f;   // Set to the middle of the screen
float lastY = screenHeight / 2.0f;  // Set to the middle of the screen
const float sensitivity = 0.1f;     // Sensitivity of mouse movement
float cameraYaw = -90.0;
float cameraPitch = 0.0f;

void Application::OnMouseMove(double xPos, double yPos) {
  if (firstMouse) {
    lastX = xPos;
    lastY = yPos;
    firstMouse = false;
  }

  float xoffset = lastX - xPos;
  float yoffset = lastY - yPos;
  lastX = xPos;
  lastY = yPos;

  xoffset *= sensitivity;
  yoffset *= sensitivity;

  cameraYaw += xoffset;
  cameraPitch += yoffset;

  if (cameraPitch > 89.0f) {
    cameraPitch = 89.0f;
  }
  if (cameraPitch < -89.0f) {
    cameraPitch = -89.0f;
  }

  glm::vec3 front;
  front.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
  front.y = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
  front.z = sin(glm::radians(cameraPitch));
  cameraFront = glm::normalize(front);

  updateViewMatrix();
}

void Application::OnMouseClick(Rain::MouseCode button) {
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse) {
    return;
  }
}

void Application::updateViewMatrix() {
  Cam->LookAt(render->m_queue, cameraFront);
  Cam->updateBuffer(render->m_queue);
}

void Application::OnUpdate() {
  updateCameraPosition();
  render->OnFrameStart();

  Cam->Begin(render->renderPass);
  for (auto v : renderMeshes) {
		render->SetPipeline(m_pipeline);
    v->SetRenderPass(render->renderPass);
    wgpuRenderPassEncoderDraw(render->renderPass, v->meshVertexCount, 1, 0, 0);
  }

  updateGui(render->renderPass);
  render->OnFrameEnd();
}

void Application::OnKeyPressed(KeyCode key, KeyAction action) {
  if (key == Rain::Key::F && action == Rain::Key::RN_KEY_RELEASE) {
    if (!switchKey) {
      switchKey = true;
    }
  }

  if (action == Rain::Key::RN_KEY_PRESS) {
    keyStates[key] = true;
  } else if (action == Rain::Key::RN_KEY_RELEASE) {
    keyStates[key] = false;
  }
}

void Application::updateCameraPosition() {
  const float cameraSpeed = 0.05f;
  glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFront, Cam->cameraUp));  // Corrected order for cross product

  if (keyStates[GLFW_KEY_W]) {
    Cam->cameraPos += cameraSpeed * cameraFront;
  }
  if (keyStates[GLFW_KEY_S]) {
    Cam->cameraPos -= cameraSpeed * cameraFront;
  }
  if (keyStates[GLFW_KEY_A]) {
    Cam->cameraPos -= cameraSpeed * cameraRight;  // Corrected direction for left movement
  }
  if (keyStates[GLFW_KEY_D]) {
    Cam->cameraPos += cameraSpeed * cameraRight;
  }
  if (keyStates[GLFW_KEY_SPACE]) {
    Cam->cameraPos += cameraSpeed * Cam->cameraUp;
  }
  if (keyStates[GLFW_KEY_LEFT_SHIFT]) {
    Cam->cameraPos -= cameraSpeed * Cam->cameraUp;
  }

  updateViewMatrix();
}

void Application::updateGui(WGPURenderPassEncoder renderPass) {
  ImGui_ImplWGPU_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::Begin("Hello, world!");

  ImGuiIO& io = ImGui::GetIO();
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
              1000.0f / io.Framerate, io.Framerate);
  ImGui::End();

  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Save Map")) {
      }
      if (ImGui::MenuItem("Load Map")) {
      }
      ImGui::EndMenu();
    }
  }
  ImGui::EndMainMenuBar();

  ImGui::EndFrame();
  ImGui::Render();
  ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
}
