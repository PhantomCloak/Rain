#include "SceneRenderer.h"
#include "Application.h"
#include "render/Render.h"

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

void SceneRenderer::SubmitMesh(Ref<MeshSource> meshSource, uint32_t submeshIndex, uint32_t materialIndex, glm::mat4& transform) {
  MeshKey meshKey = {meshSource->Id, materialIndex, submeshIndex};

  auto& transformStorage = m_MeshTransformMap[meshKey].Transforms.emplace_back();

  transformStorage.MRow[0] = {transform[0][0], transform[1][0], transform[2][0], transform[3][0]};
  transformStorage.MRow[1] = {transform[0][1], transform[1][1], transform[2][1], transform[3][1]};
  transformStorage.MRow[2] = {transform[0][2], transform[1][2], transform[2][2], transform[3][2]};

  auto& drawCommand = m_DrawList[meshKey];

  drawCommand.Mesh = meshSource;
  drawCommand.SubmeshIndex = submeshIndex;
  drawCommand.MaterialIndex = materialIndex;
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

  VertexBufferLayout instanceLayout = {48, {
      {4, ShaderDataType::Float4, "a_MRow0", 0},
      {5, ShaderDataType::Float4, "a_MRow1", 16},
      {6, ShaderDataType::Float4, "a_MRow2", 32}}};
  // clang-format on

  GroupLayout sceneGroup = {
      {0, GroupLayoutVisibility::Both, GroupLayoutType::Uniform}};

  GroupLayout materialGroup = {
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {1, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler},
      {2, GroupLayoutVisibility::Both, GroupLayoutType::Uniform}};

  GroupLayout shadowGroup = {
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::TextureDepth},
      {1, GroupLayoutVisibility::Fragment, GroupLayoutType::SamplerCompare}};

  GroupLayout ppfxGroup = {
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {1, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler}};

  static std::map<int, GroupLayout> groupLayouts, groupLayoutsShadow;

  groupLayouts.insert({0, sceneGroup});
  groupLayouts.insert({1, materialGroup});
  groupLayouts.insert({2, shadowGroup});

	groupLayoutsShadow.insert({0, sceneGroup});

  m_ShaderManager->LoadShader("SH_DefaultBasicBatch", RESOURCE_DIR "/shaders/default_basic_batch.wgsl");
  m_ShaderManager->LoadShader("SH_Shadow", RESOURCE_DIR "/shaders/shadow_map.wgsl");

  RenderPipelineProps litPipeProps = {
      .VertexLayout = vertexLayout,
      .InstanceLayout = instanceLayout,
      .CullingMode = PipelineCullingMode::NONE,
      .VertexShader = m_ShaderManager->GetShader("SH_DefaultBasicBatch"),
      .FragmentShader = m_ShaderManager->GetShader("SH_DefaultBasicBatch"),
      .ColorFormat = TextureFormat::RGBA,
      .DepthFormat = TextureFormat::Depth,
      .groupLayout = groupLayouts};

  RenderPipelineProps shadowPipeProps = {
      .VertexLayout = vertexLayout,
      .InstanceLayout = instanceLayout,
      .CullingMode = PipelineCullingMode::BACK,
      .VertexShader = m_ShaderManager->GetShader("SH_Shadow"),
      .FragmentShader = m_ShaderManager->GetShader("SH_Shadow"),
      .ColorFormat = TextureFormat::Undefined,
      .DepthFormat = TextureFormat::Depth,
      .groupLayout = groupLayoutsShadow};

  glm::vec2 screenSize = Application::Get()->GetWindowSize();

  m_LitDepthTexture = Texture::Create({screenSize, TextureFormat::Depth});
  m_ShadowMapTexture = Texture::Create({{4096, 4096}, TextureFormat::Depth});

  m_LitPipeline = CreateRef<RenderPipeline>("RP_Lit", litPipeProps, nullptr, m_LitDepthTexture);
  m_ShadowPipeline = CreateRef<RenderPipeline>("RP_Shadow", shadowPipeProps, nullptr, m_ShadowMapTexture);

  Ref<RenderContext> renderContext = Render::Instance->GetRenderContext();

  WGPUBindGroupLayout bglCamera = wgpuRenderPipelineGetBindGroupLayout(m_LitPipeline->GetPipeline(), 0);

  m_SceneUniformBuffer = GPUAllocator::GAlloc(WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, sizeof(SceneUniform));

  std::vector<WGPUBindGroupEntry> bgEntriesScene(1);

  bgEntriesScene[0].binding = 0;
  bgEntriesScene[0].buffer = m_SceneUniformBuffer->Buffer;
  bgEntriesScene[0].offset = 0;
  bgEntriesScene[0].size = sizeof(SceneUniform);

  static WGPUBindGroupDescriptor bgDescScene = {};
  bgDescScene.label = "bg_cam";
  bgDescScene.layout = bglCamera;
  bgDescScene.entryCount = (uint32_t)bgEntriesScene.size();
  bgDescScene.entries = bgEntriesScene.data();

  m_SceneBindGroup = wgpuDeviceCreateBindGroup(renderContext->GetDevice(), &bgDescScene);

  WGPUSamplerDescriptor shadowSamplerDesc = {};  // Zero-initialize the struct

  // Set the properties
  shadowSamplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
  shadowSamplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
  shadowSamplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;

  shadowSamplerDesc.minFilter = WGPUFilterMode_Nearest;
  shadowSamplerDesc.magFilter = WGPUFilterMode_Linear;

  shadowSamplerDesc.compare = WGPUCompareFunction_Less;
  shadowSamplerDesc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
  shadowSamplerDesc.compare = WGPUCompareFunction_Less,
  shadowSamplerDesc.lodMinClamp = 0.0f;
  shadowSamplerDesc.lodMaxClamp = 1.0f;
  shadowSamplerDesc.maxAnisotropy = 1;

  WGPUSampler shadowSampler = wgpuDeviceCreateSampler(RenderContext::GetDevice(), &shadowSamplerDesc);

  WGPUBindGroupLayout bglShadow = wgpuRenderPipelineGetBindGroupLayout(m_LitPipeline->GetPipeline(), 2);

  std::vector<WGPUBindGroupEntry> bgEntriesShadowMap(2);

  bgEntriesShadowMap[0].binding = 0;
  bgEntriesShadowMap[0].offset = 0;
  bgEntriesShadowMap[0].textureView = m_ShadowPipeline->GetDepthAttachment()->View;

  bgEntriesShadowMap[1].binding = 1;
  bgEntriesShadowMap[1].offset = 0;
  bgEntriesShadowMap[1].sampler = shadowSampler;

  static WGPUBindGroupDescriptor bgDescShadow = {};
  bgDescShadow.label = "bg_shadow";
  bgDescShadow.layout = bglShadow;
  bgDescShadow.entryCount = (uint32_t)bgEntriesShadowMap.size();
  bgDescShadow.entries = bgEntriesShadowMap.data();

  m_ShadowPassBindGroup = wgpuDeviceCreateBindGroup(renderContext->GetDevice(), &bgDescShadow);

  const float shadowFrustum = 200;
  m_SceneUniform.shadowViewProjection = glm::ortho(-shadowFrustum,
                                                   shadowFrustum,
                                                   -shadowFrustum,
                                                   shadowFrustum,
                                                   0.0f,
                                                   1500.0f) *
                                        GetViewMatrix(glm::vec3(212, 852, 71), glm::vec3(-107, 35, 0));

  m_SceneUniformBuffer->SetData(&m_SceneUniform, sizeof(SceneUniform));
}

void SceneRenderer::PreRender() {
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

WGPURenderPassEncoder BeginRenderPass(Ref<RenderPipeline> pipe, WGPUCommandEncoder encoder) {
  WGPURenderPassDescriptor passDesc{.label = pipe->GetName().c_str()};

  if (pipe->HasColorAttachment()) {
    WGPURenderPassColorAttachment colorAttachment{};
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    colorAttachment.clearValue = WGPUColor{0.52, 0.80, 0.92, 1};
    colorAttachment.resolveTarget = nullptr;
    colorAttachment.view = pipe->GetColorAttachment()->View;

    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAttachment;
  } else {
    passDesc.colorAttachmentCount = 0;
    passDesc.colorAttachments = nullptr;
  }

  WGPURenderPassDepthStencilAttachment depthAttachment;
  depthAttachment.view = pipe->GetDepthAttachment()->View;
  depthAttachment.depthClearValue = 1.0f;
  depthAttachment.depthLoadOp = WGPULoadOp_Clear;
  depthAttachment.depthStoreOp = WGPUStoreOp_Store;
  depthAttachment.depthReadOnly = false;
  depthAttachment.stencilClearValue = 0;
  depthAttachment.stencilLoadOp = WGPULoadOp_Undefined;
  depthAttachment.stencilStoreOp = WGPUStoreOp_Undefined;
  depthAttachment.stencilReadOnly = true;

  passDesc.depthStencilAttachment = &depthAttachment;

  return wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
}

void SceneRenderer::SetScene(Scene* scene) {
  RN_ASSERT(scene != nullptr, "Scene cannot be null");
  m_Scene = scene;
}

void SceneRenderer::BeginScene(const SceneCamera& camera) {
  m_SceneUniform.viewProjection = camera.Projection * camera.ViewMatrix;
  m_SceneUniform.cameraViewMatrix = camera.ViewMatrix;
  m_SceneUniformBuffer->SetData(&m_SceneUniform, sizeof(SceneUniform));
}

void EndRenderPass(WGPURenderPassEncoder encoder) {
  wgpuRenderPassEncoderEnd(encoder);
}

void SceneRenderer::FlushDrawList() {
  WGPUCommandEncoderDescriptor commandEncoderDesc = {.label = "Command Encoder"};
  Ref<RenderContext> renderContext = Render::Instance->GetRenderContext();

  auto encoder = wgpuDeviceCreateCommandEncoder(renderContext->GetDevice(), &commandEncoderDesc);

  auto shadowPass = BeginRenderPass(m_ShadowPipeline, encoder);
  wgpuRenderPassEncoderSetBindGroup(shadowPass, 0, m_SceneBindGroup, 0, nullptr);
  for (auto& [mk, dc] : m_DrawList) {
    Ref<Material> mat = dc.Mesh->m_Materials[dc.MaterialIndex];
    Render::Instance->RenderMesh(shadowPass, m_ShadowPipeline->GetPipeline(), dc.Mesh, dc.SubmeshIndex, mat, m_TransformBuffer, m_MeshTransformMap[mk].TransformOffset, dc.InstanceCount);
  }
  EndRenderPass(shadowPass);

  m_LitPipeline->SetColorAttachment(Render::Instance->GetCurrentSwapChainTexture());
  auto litPass = BeginRenderPass(m_LitPipeline, encoder);
  wgpuRenderPassEncoderSetBindGroup(litPass, 0, m_SceneBindGroup, 0, nullptr);
  wgpuRenderPassEncoderSetBindGroup(litPass, 2, m_ShadowPassBindGroup, 0, nullptr);
  for (auto& [mk, dc] : m_DrawList) {
    Ref<Material> mat = dc.Mesh->m_Materials[dc.MaterialIndex];
    Render::Instance->RenderMesh(litPass, m_LitPipeline->GetPipeline(), dc.Mesh, dc.SubmeshIndex, mat, m_TransformBuffer, m_MeshTransformMap[mk].TransformOffset, dc.InstanceCount);
  }
  EndRenderPass(litPass);

  WGPUCommandBufferDescriptor cmdBufferDescriptor = {.label = "Command Buffer"};
  WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);

  wgpuQueueSubmit(*renderContext->GetQueue(), 1, &commandBuffer);

  wgpuCommandEncoderRelease(encoder);
  wgpuTextureViewRelease(m_LitPipeline->GetColorAttachment()->View);

#ifndef __EMSCRIPTEN__
  wgpuSwapChainPresent(Render::Instance->m_swapChain);
  wgpuDeviceTick(renderContext->GetDevice());
#endif

  m_DrawList.clear();
  m_MeshTransformMap.clear();
}

void SceneRenderer::EndScene() {
  PreRender();
  FlushDrawList();
}
