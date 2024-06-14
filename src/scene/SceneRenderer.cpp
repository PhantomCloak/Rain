#include "SceneRenderer.h"
#include "Application.h"
#include "render/Render.h"

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

  // TODO: We could abstract this in similar fashion with GroupLayout
  static std::vector<WGPUVertexAttribute> vaDefault(4);

  vaDefault[0] = {};
  vaDefault[0].shaderLocation = 0;
  vaDefault[0].offset = 0;
  vaDefault[0].format = WGPUVertexFormat_Float32x3;

  vaDefault[1] = {};
  vaDefault[1].shaderLocation = 1;
  vaDefault[1].offset = 16;
  vaDefault[1].format = WGPUVertexFormat_Float32x3;

  vaDefault[2] = {};
  vaDefault[2].shaderLocation = 2;
  vaDefault[2].offset = 32;
  vaDefault[2].format = WGPUVertexFormat_Float32x2;

  vaDefault[3] = {};
  vaDefault[3].shaderLocation = 3;
  vaDefault[3].offset = 48;
  vaDefault[3].format = WGPUVertexFormat_Float32x3;

  static std::vector<WGPUVertexAttribute> vaInstance(3);

  vaInstance[0] = {};
  vaInstance[0].shaderLocation = 4;
  vaInstance[0].offset = 0;
  vaInstance[0].format = WGPUVertexFormat_Float32x4;

  vaInstance[1] = {};
  vaInstance[1].shaderLocation = 5;
  vaInstance[1].offset = 16;
  vaInstance[1].format = WGPUVertexFormat_Float32x4;

  vaInstance[2] = {};
  vaInstance[2].shaderLocation = 6;
  vaInstance[2].offset = 32;
  vaInstance[2].format = WGPUVertexFormat_Float32x4;

  WGPUVertexBufferLayout vblInstance = {};
  vblInstance.attributeCount = vaInstance.size();
  vblInstance.attributes = vaInstance.data();
  vblInstance.arrayStride = 48;
  vblInstance.stepMode = WGPUVertexStepMode_Instance;

  WGPUVertexBufferLayout vblDefault = {};
  vblDefault.attributeCount = vaDefault.size();
  vblDefault.attributes = vaDefault.data();
  vblDefault.arrayStride = 64;

  vblDefault.stepMode = WGPUVertexStepMode_Vertex;
  static std::vector<WGPUVertexBufferLayout> vertexLayouts;
  vertexLayouts.push_back(vblDefault);
  vertexLayouts.push_back(vblInstance);

  GroupLayout sceneGroup = {
      {0, GroupLayoutVisibility::Vertex, GroupLayoutType::StorageReadOnly}};

  GroupLayout cameraGroup = {
      {0, GroupLayoutVisibility::Both, GroupLayoutType::Uniform}};

  GroupLayout materialGroup = {
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {1, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler},
      {2, GroupLayoutVisibility::Both, GroupLayoutType::Uniform}};

  GroupLayout shadowGroup = {
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::TextureDepth},
      {1, GroupLayoutVisibility::Fragment, GroupLayoutType::SamplerCompare},
      {2, GroupLayoutVisibility::Both, GroupLayoutType::Uniform}};

  GroupLayout ppfxGroup = {
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {1, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler}};

  static std::map<int, GroupLayout> groupLayouts;
  groupLayouts.insert({0, cameraGroup});
  groupLayouts.insert({1, materialGroup});

  m_ShaderManager->LoadShader("SH_DefaultBasicBatch", RESOURCE_DIR "/shaders/default_basic_batch.wgsl");

  RenderPipelineProps litPipeProps = {
      .CullingMode = PipelineCullingMode::BACK,
      .VertexShader = m_ShaderManager->GetShader("SH_DefaultBasicBatch"),
      .FragmentShader = m_ShaderManager->GetShader("SH_DefaultBasicBatch"),
      .ColorFormat = TextureFormat::RGBA,
      .DepthFormat = TextureFormat::Dept};

  Ref<RenderPipeline>
      litPipe = CreateRef<RenderPipeline>("RP_Lit", litPipeProps, vertexLayouts, groupLayouts);

  glm::vec2 screenSize = Application::Get()->GetWindowSize();
  litPipe->pipelineInfo.m_ColorAttachment = Texture::Create({screenSize, TextureFormat::RGBA});
  litPipe->pipelineInfo.m_DepthTexture = Texture::Create({screenSize, TextureFormat::Dept});

  m_RenderPipelines["RP_Lit"] = litPipe;

  Ref<RenderContext> renderContext = Render::Instance->GetRenderContext();

  WGPUBindGroupLayout bglCamera = wgpuRenderPipelineGetBindGroupLayout(m_RenderPipelines["RP_Lit"]->GetPipeline(), 0);

  m_SceneUniformBuffer = GPUAllocator::GAlloc(WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, sizeof(SceneUniform));

  std::vector<WGPUBindGroupEntry> bgEntriesCamera(1);

  bgEntriesCamera[0].binding = 0;
  bgEntriesCamera[0].buffer = m_SceneUniformBuffer->Buffer;
  bgEntriesCamera[0].offset = 0;
  bgEntriesCamera[0].size = sizeof(SceneUniform);

  static WGPUBindGroupDescriptor bgDescDefaultCamera = {};
  bgDescDefaultCamera.label = "bg_cam";
  bgDescDefaultCamera.layout = bglCamera;
  bgDescDefaultCamera.entryCount = (uint32_t)bgEntriesCamera.size();
  bgDescDefaultCamera.entries = bgEntriesCamera.data();

  m_SceneBindGroup = wgpuDeviceCreateBindGroup(renderContext->GetDevice(), &bgDescDefaultCamera);

  //CameraHack(litPipe->GetPipeline());
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

  wgpuQueueWriteBuffer(Render::Instance->m_queue, m_TransformBuffer->Buffer, 0, submeshTransforms, offset * sizeof(TransformVertexData));
}

WGPURenderPassEncoder BeginRenderPass(std::string name, Ref<RenderPipeline> pipe, WGPUCommandEncoder encoder, WGPUTextureView target) {
  WGPURenderPassColorAttachment colorAttachment{};
  colorAttachment.loadOp = WGPULoadOp_Clear;
  colorAttachment.storeOp = WGPUStoreOp_Store;
  colorAttachment.clearValue = WGPUColor{0.52, 0.80, 0.92, 1};
  colorAttachment.resolveTarget = nullptr;
  colorAttachment.view = target;

  WGPURenderPassDepthStencilAttachment depthAttachment;
  depthAttachment.view = pipe->pipelineInfo.m_DepthTexture->View;
  depthAttachment.depthClearValue = 1.0f;
  depthAttachment.depthLoadOp = WGPULoadOp_Clear;
  depthAttachment.depthStoreOp = WGPUStoreOp_Store;
  depthAttachment.depthReadOnly = false;
  depthAttachment.stencilClearValue = 0;
  depthAttachment.stencilLoadOp = WGPULoadOp_Undefined;
  depthAttachment.stencilStoreOp = WGPUStoreOp_Undefined;
  depthAttachment.stencilReadOnly = true;

  WGPURenderPassDescriptor passDesc{.label = "first_pass"};
  passDesc.colorAttachmentCount = 1;
  passDesc.colorAttachments = &colorAttachment;
  passDesc.depthStencilAttachment = &depthAttachment;

  return wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
}

void SceneRenderer::SetScene(Scene* scene) {
  RN_ASSERT(scene != nullptr, "Scene cannot be null");
  m_Scene = scene;
}

void SceneRenderer::BeginScene(const SceneCamera& camera) {
  m_SceneUniform.viewMatrix = camera.ViewMatrix;
  m_SceneUniform.projectionMatrix = camera.Projection;

  m_SceneUniformBuffer->SetData(&m_SceneUniform, sizeof(SceneUniform));
}

void EndRenderPass(WGPURenderPassEncoder encoder) {
  wgpuRenderPassEncoderEnd(encoder);
}

void SceneRenderer::FlushDrawList() {
  WGPUCommandEncoderDescriptor commandEncoderDesc = {.label = "Command Encoder"};
  Ref<RenderContext> renderContext = Render::Instance->GetRenderContext();

  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(renderContext->GetDevice(), &commandEncoderDesc);
  WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(Render::Instance->m_swapChain);

  auto& litPipeline = m_RenderPipelines["RP_Lit"];
  auto litPass = BeginRenderPass("Default", litPipeline, encoder, nextTexture);

  wgpuRenderPassEncoderSetBindGroup(litPass, 0, m_SceneBindGroup, 0, nullptr);
  for (auto& [mk, dc] : m_DrawList) {
    Ref<Material> mat = dc.Mesh->m_Materials[dc.MaterialIndex];
    Render::Instance->RenderMesh(litPass, litPipeline->GetPipeline(), dc.Mesh, dc.SubmeshIndex, mat, m_TransformBuffer, m_MeshTransformMap[mk].TransformOffset, dc.InstanceCount);
  }
  EndRenderPass(litPass);

  WGPUCommandBufferDescriptor cmdBufferDescriptor = {.label = "Command Buffer"};
  WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);

  wgpuQueueSubmit(*renderContext->GetQueue(), 1, &commandBuffer);

  wgpuCommandEncoderRelease(encoder);
  wgpuTextureViewRelease(nextTexture);

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
