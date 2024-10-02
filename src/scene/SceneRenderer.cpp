#include "SceneRenderer.h"
#include "Application.h"
#include "debug/Profiler.h"
#include "render/Render.h"
#include "render/ResourceManager.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include "imgui.h"

SceneRenderer* SceneRenderer::instance;

glm::mat4 GetViewMatrix(glm::vec3 pos, glm::vec3 rot) {
  glm::vec3 shadowCameraPos = pos;
  glm::vec3 shadowCameraRot = rot;

  float pitch = shadowCameraRot.x;
  float yaw = shadowCameraRot.y;
  float roll = shadowCameraRot.z;

  glm::vec3 front;
  front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
  front.y = sin(glm::radians(pitch));
  front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

  glm::vec3 Front = glm::normalize(front);
  glm::vec3 worldUp = glm::vec3(0, 1, 0);
  glm::vec3 right = glm::normalize(glm::cross(Front, worldUp));
  glm::vec3 up = glm::rotate(glm::mat4(1.0f), glm::radians(roll), right) * glm::vec4(worldUp, 1.0);

  return glm::lookAt(shadowCameraPos, shadowCameraPos + Front, up);
}

void SceneRenderer::SubmitMesh(Ref<MeshSource> meshSource, uint32_t submeshIndex, Ref<MaterialTable> materialTable, glm::mat4& transform) {
  const auto& submesh = meshSource->m_SubMeshes[submeshIndex];
  const auto materialHandle = materialTable->HasMaterial(submesh.MaterialIndex) ? materialTable->GetMaterial(submesh.MaterialIndex) : meshSource->Materials->GetMaterial(submesh.MaterialIndex);

  MeshKey meshKey = {meshSource->Id, materialHandle->Id, submeshIndex};

  auto& transformStorage = m_MeshTransformMap[meshKey].Transforms.emplace_back();

  transformStorage.MRow[0] = {transform[0][0], transform[1][0], transform[2][0], transform[3][0]};
  transformStorage.MRow[1] = {transform[0][1], transform[1][1], transform[2][1], transform[3][1]};
  transformStorage.MRow[2] = {transform[0][2], transform[1][2], transform[2][2], transform[3][2]};

  auto& drawCommand = m_DrawList[meshKey];

  drawCommand.Mesh = meshSource;
  drawCommand.SubmeshIndex = submeshIndex;
  drawCommand.Materials = materialTable;
  drawCommand.InstanceCount++;
}

void SceneRenderer::Init() {
  m_TransformBuffer = GPUAllocator::GAlloc("scene_global_transform", WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex, 1024 * sizeof(TransformVertexData));
  m_SceneUniform = {};

  // clang-format off
	VertexBufferLayout vertexLayout = {64, {
			{0, ShaderDataType::Float3, "position", 0},
			{1, ShaderDataType::Float3, "normal", 16},
			{2, ShaderDataType::Float2, "uv", 32},
			{3, ShaderDataType::Float3, "tangent", 48}}};

	VertexBufferLayout vertexLayoutQuad = {32, {
			{0, ShaderDataType::Float3, "position", 0},
			{1, ShaderDataType::Float2, "uv", 16}}};

  VertexBufferLayout instanceLayout = {48, {
      {4, ShaderDataType::Float4, "a_MRow0", 0},
      {5, ShaderDataType::Float4, "a_MRow1", 16},
      {6, ShaderDataType::Float4, "a_MRow2", 32}}};

  // clang-format on

  auto defaultShader = m_ShaderManager->LoadShader("SH_DefaultBasicBatch", RESOURCE_DIR "/shaders/default.wgsl");
  auto shadowShader = m_ShaderManager->LoadShader("SH_Shadow", RESOURCE_DIR "/shaders/shadow_map.wgsl");
  auto skyboxShader = m_ShaderManager->LoadShader("SH_Skybox", RESOURCE_DIR "/shaders/skybox.wgsl");
  // auto ppfxShader = m_ShaderManager->LoadShader("SH_Ppfx", RESOURCE_DIR "/shaders/ppfx.wgsl");

  glm::vec2 screenSize = Application::Get()->GetWindowSize();

  m_LitDepthTexture = Texture::Create({screenSize, TextureFormat::Depth});
  m_LitDepthTexture = Texture::Create({screenSize, TextureFormat::Depth});

  m_ShadowDepthTexture = Texture::Create({{4096, 4096}, TextureFormat::Depth});
  m_ShadowSampler = Sampler::Create({.Name = "ShadowSampler",
                                     .WrapFormat = TextureWrappingFormat::ClampToEdges,
                                     .MagFilterFormat = FilterMode::Linear,
                                     .MinFilterFormat = FilterMode::Nearest,
                                     .MipFilterFormat = FilterMode::Nearest,
                                     .Compare = CompareMode::Less,
                                     .LodMinClamp = 1.0f,
                                     .LodMaxClamp = 1.0f});

  m_LitPassTexture = Texture::Create({screenSize, TextureFormat::RGBA});
  // m_PpfxSampler = Sampler::Create({.Name = "PpfxSampler",
  //                                  .WrapFormat = TextureWrappingFormat::ClampToEdges,
  //                                  .MagFilterFormat = FilterMode::Linear,
  //                                  .MinFilterFormat = FilterMode::Linear,
  //                                  .MipFilterFormat = FilterMode::Linear,
  //                                  .Compare = CompareMode::CompareUndefined,
  //                                  .LodMinClamp = 0.0f,
  //                                  .LodMaxClamp = 1.0f});

  RenderPipelineProps shadowPipeProps = {
      .VertexLayout = vertexLayout,
      .InstanceLayout = instanceLayout,
      .CullingMode = PipelineCullingMode::BACK,
      .VertexShader = shadowShader,
      .FragmentShader = shadowShader,
      .ColorFormat = TextureFormat::Undefined,
      .DepthFormat = TextureFormat::Depth,
      .TargetDepthBuffer = m_ShadowDepthTexture};

  RenderPipelineProps litPipeProps = {
      .VertexLayout = vertexLayout,
      .InstanceLayout = instanceLayout,
      .CullingMode = PipelineCullingMode::NONE,
      .VertexShader = defaultShader,
      .FragmentShader = defaultShader,
      .ColorFormat = TextureFormat::RGBA,
      .DepthFormat = TextureFormat::Depth,
      .TargetDepthBuffer = m_LitDepthTexture};
  //.TargetFrameBuffer = m_LitPassTexture,
  //.TargetDepthBuffer = m_LitDepthTexture};

  RenderPipelineProps skyboxPipeProps = {
      .VertexLayout = vertexLayoutQuad,
      .InstanceLayout = {},
      .CullingMode = PipelineCullingMode::NONE,
      .VertexShader = skyboxShader,
      .FragmentShader = skyboxShader,
      .ColorFormat = TextureFormat::RGBA,
      .DepthFormat = TextureFormat::Undefined};

  m_LitPipeline = RenderPipeline::Create("RP_Lit", litPipeProps);
  m_ShadowPipeline = RenderPipeline::Create("RP_Shadow", shadowPipeProps);
	// m_PpfxPipeline = RenderPipeline::Create("RP_PPFX", ppfxPipeProps);
  m_SkyboxPipeline = RenderPipeline::Create("RP_Skybox", skyboxPipeProps);

  const float shadowFrustum = 200;
  m_SceneUniform.shadowViewProjection = glm::ortho(-shadowFrustum,
                                                   shadowFrustum,
                                                   -shadowFrustum,
                                                   shadowFrustum,
                                                   0.0f,
                                                   1500.0f) *
                                        GetViewMatrix(glm::vec3(212, 852, 71), glm::vec3(-107, 35, 0));

  m_SceneUniformBuffer = GPUAllocator::GAlloc(WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, sizeof(SceneUniform));
  m_SceneUniformBuffer->SetData(&m_SceneUniform, sizeof(SceneUniform));

  auto skyboxSample = Sampler::Create({.Name = "S_Skybox",
                                       .WrapFormat = TextureWrappingFormat::ClampToEdges,
                                       .MagFilterFormat = FilterMode::Linear,
                                       .MinFilterFormat = FilterMode::Linear,
                                       .MipFilterFormat = FilterMode::Linear,
                                       .Compare = CompareMode::CompareUndefined,
                                       .LodMinClamp = 0.0f,
                                       .LodMaxClamp = 1.0f});

  Ref<Texture> skybox = Rain::ResourceManager::LoadCubeTexture("T3D_Skybox",
                                                               {RESOURCE_DIR "/textures/right.jpg",
                                                                RESOURCE_DIR "/textures/left.jpg",
                                                                RESOURCE_DIR "/textures/top.jpg",
                                                                RESOURCE_DIR "/textures/bottom.jpg",
                                                                RESOURCE_DIR "/textures/front.jpg",
                                                                RESOURCE_DIR "/textures/back.jpg"});

  RenderPassProps propShadowPass = {
      .Pipeline = m_ShadowPipeline,
      .DebugName = "ShadowPass"};

  m_ShadowPass = RenderPass::Create(propShadowPass);
  m_ShadowPass->Set("u_scene", m_SceneUniformBuffer);
  m_ShadowPass->Bake();

  RenderPassProps propLitPass = {
      .Pipeline = m_LitPipeline,
      .DebugName = "LitPass"};

  m_LitPass = RenderPass::Create(propLitPass);
  m_LitPass->Set("u_scene", m_SceneUniformBuffer);
	m_LitPass->Set("irradianceMap", skybox);
	m_LitPass->Set("irradianceMapSampler", skyboxSample);
  //  m_LitPass->Set("shadowMap", m_ShadowPass->GetDepthOutput());
  //  m_LitPass->Set("shadowMap", m_ShadowDepthTexture);
  //  m_LitPass->Set("shadowSampler", m_ShadowSampler);
  m_LitPass->Bake();

  RenderPassProps propSkyboxPass = {
      .Pipeline = m_SkyboxPipeline,
      .DebugName = "SykboxPass"};

  m_CameraUniformBuffer = GPUAllocator::GAlloc(WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, sizeof(CameraData));
  m_CameraUniformBuffer->SetData(&m_CameraData, sizeof(CameraData));

  m_SkyboxPass = RenderPass::Create(propSkyboxPass);
  m_SkyboxPass->Set("cubemapTexture", skybox);
  m_SkyboxPass->Set("textureSampler", skyboxSample);
  m_SkyboxPass->Set("u_Camera", m_CameraUniformBuffer);
  m_SkyboxPass->Bake();

  RenderPassProps propPpfxPass = {
      .Pipeline = m_PpfxPipeline,
      .DebugName = "PpfxPass"};

  // m_PpfxPass = RenderPass::Create(propPpfxPass);
  // m_PpfxPass->Set("renderTexture", m_LitPass->GetOutput(0));
  // m_PpfxPass->Set("textureSampler", m_PpfxSampler);
  // m_PpfxPass->Bake();

  auto renderContext = Render::Instance->GetRenderContext();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO();

  ImGui_ImplGlfw_InitForOther((GLFWwindow*)Application::Get()->GetNativeWindow(), true);

  ImGui_ImplWGPU_InitInfo initInfo;
  initInfo.Device = RenderContext::GetDevice();
  // initInfo.RenderTargetFormat = render->m_swapChainFormat;
  initInfo.RenderTargetFormat = WGPUTextureFormat_BGRA8Unorm;
  initInfo.DepthStencilFormat = WGPUTextureFormat_Depth24Plus;
  ImGui_ImplWGPU_Init(&initInfo);
}

void SceneRenderer::PreRender() {
  RN_PROFILE_FUNC;
  static TransformVertexData* submeshTransforms = (TransformVertexData*)malloc(1024 * sizeof(TransformVertexData));

  uint32_t offset = 0;
  for (auto& [key, transformData] : m_MeshTransformMap) {
    transformData.TransformOffset = offset * sizeof(TransformVertexData);
    for (const auto& transform : transformData.Transforms) {
      submeshTransforms[offset] = (transform);
      offset++;
    }
  }

  m_TransformBuffer->SetData(submeshTransforms, offset * sizeof(TransformVertexData));
}

void SceneRenderer::SetScene(Scene* scene) {
  RN_ASSERT(scene != nullptr, "Scene cannot be null");
  m_Scene = scene;
}

void SceneRenderer::BeginScene(const SceneCamera& camera) {
  RN_PROFILE_FUNC;
  const glm::mat4 viewInverse = glm::inverse(camera.ViewMatrix);
  const glm::vec3 cameraPosition = viewInverse[3];

  m_SceneUniform.viewProjection = camera.Projection * camera.ViewMatrix;
  m_SceneUniform.cameraViewMatrix = camera.ViewMatrix;
  m_SceneUniform.LightPosition = m_Scene->SceneLightInfo.LightPos;
  m_SceneUniform.CameraPosition = cameraPosition;

  glm::mat4 skyboxViewMatrix = camera.ViewMatrix;
  skyboxViewMatrix[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

  m_CameraData.InverseViewProjectionMatrix = glm::inverse(skyboxViewMatrix) * glm::inverse(camera.Projection);

  m_SceneUniformBuffer->SetData(&m_SceneUniform, sizeof(SceneUniform));
  m_CameraUniformBuffer->SetData(&m_CameraData, sizeof(CameraData));

  ImGui_ImplWGPU_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void SceneRenderer::FlushDrawList() {
  RN_PROFILE_FUNC;
  WGPUCommandEncoderDescriptor commandEncoderDesc = {.nextInChain = nullptr, .label = "Default Command Encoder"};
  Ref<RenderContext> renderContext = Render::Instance->GetRenderContext();

  auto commandEncoder = wgpuDeviceCreateCommandEncoder(renderContext->GetDevice(), &commandEncoderDesc);

  auto skyboxPassEncoder = Render::BeginRenderPass(m_SkyboxPass, commandEncoder);
  Render::Instance->SubmitFullscreenQuad(skyboxPassEncoder, m_SkyboxPipeline->GetPipeline());
  Render::EndRenderPass(m_SkyboxPass, skyboxPassEncoder);

  // auto shadowPassEncoder = Render::BeginRenderPass(m_ShadowPass, commandEncoder);
  // for (auto& [mk, dc] : m_DrawList) {
  //   Render::Instance->RenderMesh(shadowPassEncoder, m_ShadowPipeline->GetPipeline(), dc.Mesh, dc.SubmeshIndex, dc.Materials, m_TransformBuffer, m_MeshTransformMap[mk].TransformOffset, dc.InstanceCount);
  // }
  // Render::EndRenderPass(m_ShadowPass, shadowPassEncoder);

  auto litPassEncoder = Render::BeginRenderPass(m_LitPass, commandEncoder);
  for (auto& [mk, dc] : m_DrawList) {
    Render::Instance->RenderMesh(litPassEncoder, m_LitPipeline->GetPipeline(), dc.Mesh, dc.SubmeshIndex, dc.Materials, m_TransformBuffer, m_MeshTransformMap[mk].TransformOffset, dc.InstanceCount);
  }
  ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), litPassEncoder);
  Render::EndRenderPass(m_LitPass, litPassEncoder);

  // auto ppfxPassEncoder = Render::BeginRenderPass(m_PpfxPass, commandEncoder);
  // Render::Instance->SubmitFullscreenQuad(ppfxPassEncoder, m_PpfxPipeline->GetPipeline());
  // Render::EndRenderPass(m_PpfxPass, ppfxPassEncoder);

  WGPUCommandBufferDescriptor cmdBufferDescriptor = {.nextInChain = nullptr, .label = "Command Buffer"};
  WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(commandEncoder, &cmdBufferDescriptor);

  wgpuQueueSubmit(*renderContext->GetQueue(), 1, &commandBuffer);

  wgpuCommandEncoderRelease(commandEncoder);
  wgpuTextureViewRelease(Render::Instance->GetCurrentSwapChainTexture()->View);

#ifndef __EMSCRIPTEN__
  wgpuSwapChainPresent(Render::Instance->m_swapChain);
  wgpuDeviceTick(renderContext->GetDevice());
#endif

  m_DrawList.clear();
  m_MeshTransformMap.clear();
}

void SceneRenderer::EndScene() {
  ImGui::EndFrame();
  ImGui::Render();
  PreRender();
  FlushDrawList();
}
