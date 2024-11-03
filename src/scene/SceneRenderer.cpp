#include "SceneRenderer.h"
#include "Application.h"
#include "debug/Profiler.h"
#include "glm/gtx/rotate_vector.hpp"
#include "render/Framebuffer.h"
#include "render/Render.h"
#include "render/ResourceManager.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include "imgui.h"

SceneRenderer* SceneRenderer::instance;

std::vector<glm::vec4> SceneRenderer::getFrustumCornersWorldSpace(const glm::mat4& projview) {
  const auto inv = glm::inverse(projview);

  std::vector<glm::vec4> frustumCorners;
  for (unsigned int x = 0; x < 2; ++x) {
    for (unsigned int y = 0; y < 2; ++y) {
      for (unsigned int z = 0; z < 2; ++z) {
        const glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
        frustumCorners.push_back(pt / pt.w);
      }
    }
  }

  return frustumCorners;
}
		struct CascadeData
		{
			glm::mat4 ViewProj;
			glm::mat4 View;
			float SplitDepth;
		};
		float m_ScaleShadowCascadesToOrigin = 0.0f;

void CalculateCascades(CascadeData* cascades, const SceneCamera& sceneCamera, glm::vec3 lightDirection)
{
    float scaleToOrigin = m_ScaleShadowCascadesToOrigin;

    glm::mat4 viewMatrix = sceneCamera.ViewMatrix;
    constexpr glm::vec4 origin = glm::vec4(glm::vec3(0.0f), 1.0f);
    viewMatrix[3] = glm::lerp(viewMatrix[3], origin, scaleToOrigin);

    auto viewProjection = sceneCamera.Projection * viewMatrix;

    const int SHADOW_MAP_CASCADE_COUNT = 4;
    float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

    float nearClip = sceneCamera.Near;
    float farClip = sceneCamera.Far;
    float clipRange = farClip - nearClip;

    float minZ = nearClip;
    float maxZ = nearClip + clipRange;

    float range = maxZ - minZ;
    float ratio = maxZ / minZ;

    float CascadeSplitLambda = 0.92f;
    float CascadeFarPlaneOffset = 320.0f, CascadeNearPlaneOffset = -320.0f;

    // Calculate split depths based on view camera frustum
    for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
    {
        float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
        float log = minZ * std::pow(ratio, p);
        float uniform = minZ + range * p;
        float d = CascadeSplitLambda * (log - uniform) + uniform;
        cascadeSplits[i] = (d - nearClip) / clipRange;
    }

    cascadeSplits[3] = 0.3f;

		// Manually set cascades here
		//cascadeSplits[0] = 0.10f;
		//cascadeSplits[1] = 0.20f;
		//cascadeSplits[2] = 0.40f;
		//cascadeSplits[3] = 1.0f;


    // Calculate orthographic projection matrix for each cascade
    float lastSplitDist = 0.0;
    for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
    {
        float splitDist = cascadeSplits[i];

        glm::vec3 frustumCorners[8] =
        {
            glm::vec3(-1.0f,  1.0f, -1.0f),
            glm::vec3(1.0f,  1.0f, -1.0f),
            glm::vec3(1.0f, -1.0f, -1.0f),
            glm::vec3(-1.0f, -1.0f, -1.0f),
            glm::vec3(-1.0f,  1.0f,  1.0f),
            glm::vec3(1.0f,  1.0f,  1.0f),
            glm::vec3(1.0f, -1.0f,  1.0f),
            glm::vec3(-1.0f, -1.0f,  1.0f),
        };

        // Project frustum corners into world space
        glm::mat4 invCam = glm::inverse(viewProjection);
        for (uint32_t i = 0; i < 8; i++)
        {
            glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
            frustumCorners[i] = invCorner / invCorner.w;
        }

        for (uint32_t i = 0; i < 4; i++)
        {
            glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
            frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
            frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
        }

        // Get frustum center
        glm::vec3 frustumCenter = glm::vec3(0.0f);
        for (uint32_t i = 0; i < 8; i++)
            frustumCenter += frustumCorners[i];

        frustumCenter /= 8.0f;

        float radius = 0.0f;
        for (uint32_t i = 0; i < 8; i++)
        {
            float distance = glm::length(frustumCorners[i] - frustumCenter);
            radius = glm::max(radius, distance);
        }
        radius = std::ceil(radius * 16.0f) / 16.0f;

        glm::vec3 maxExtents = glm::vec3(radius);
        glm::vec3 minExtents = -maxExtents;

        glm::vec3 lightDir = -lightDirection;

        // Modified lightViewMatrix
        glm::mat4 lightViewMatrix = glm::lookAt(
            frustumCenter - lightDir * radius,
            frustumCenter,
            glm::vec3(0.0f, 1.0f, 0.0f));

        // Modified lightOrthoMatrix
        glm::mat4 lightOrthoMatrix = glm::ortho(
            minExtents.x, maxExtents.x,
            minExtents.y, maxExtents.y,
            minExtents.z + CascadeNearPlaneOffset,
            maxExtents.z + CascadeFarPlaneOffset);

        // Offset to texel space to avoid shimmering
        glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
        float ShadowMapResolution = 4096;

        glm::vec4 shadowOrigin = (shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) * ShadowMapResolution / 2.0f;
        glm::vec4 roundedOrigin = glm::round(shadowOrigin);
        glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
        roundOffset = roundOffset * 2.0f / ShadowMapResolution;
        roundOffset.z = 0.0f;
        roundOffset.w = 0.0f;

        lightOrthoMatrix[3] += roundOffset;

        // Modified SplitDepth calculation
        cascades[i].SplitDepth = nearClip + splitDist * clipRange;
        cascades[i].ViewProj = lightOrthoMatrix * lightViewMatrix;
        cascades[i].View = lightViewMatrix;

        lastSplitDist = cascadeSplits[i];
    }
}
std::vector<glm::vec4> SceneRenderer::getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view) {
  return getFrustumCornersWorldSpace(proj * view);
}

glm::mat4 SceneRenderer::getLightSpaceMatrix(float nearPlane, float farPlane, float zoom, float w, float h, glm::mat4 viewMat, glm::vec3 lightDir) {
  const auto proj = glm::perspective(
      glm::radians(zoom), (float)w / (float)h, nearPlane,
      farPlane);
  const auto corners = getFrustumCornersWorldSpace(proj, viewMat);

  glm::vec3 center = glm::vec3(0, 0, 0);
  for (const auto& v : corners) {
    center += glm::vec3(v);
  }
  center /= corners.size();

  const auto lightView = glm::lookAt(center + lightDir, center, glm::vec3(0.0f, 1.0, 0.0));

  float minX = std::numeric_limits<float>::max();
  float maxX = std::numeric_limits<float>::lowest();
  float minY = std::numeric_limits<float>::max();
  float maxY = std::numeric_limits<float>::lowest();
  float minZ = std::numeric_limits<float>::max();
  float maxZ = std::numeric_limits<float>::lowest();
  for (const auto& v : corners) {
    const auto trf = lightView * v;
    minX = std::min(minX, trf.x);
    maxX = std::max(maxX, trf.x);
    minY = std::min(minY, trf.y);
    maxY = std::max(maxY, trf.y);
    minZ = std::min(minZ, trf.z);
    maxZ = std::max(maxZ, trf.z);
  }

  // Tune this parameter according to the scene
  constexpr float zMult = 10.0f;
  if (minZ < 0) {
    minZ *= zMult;
  } else {
    minZ /= zMult;
  }
  if (maxZ < 0) {
    maxZ /= zMult;
  } else {
    maxZ *= zMult;
  }

  const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
  return lightProjection * lightView;
}
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

  auto pbrShader = m_ShaderManager->LoadShader("SH_DefaultBasicBatch", RESOURCE_DIR "/shaders/pbr.wgsl");
  auto shadowShader = m_ShaderManager->LoadShader("SH_Shadow", RESOURCE_DIR "/shaders/shadow_map.wgsl");
  auto skyboxShader = m_ShaderManager->LoadShader("SH_Skybox", RESOURCE_DIR "/shaders/skybox.wgsl");
  auto ppfxShader = m_ShaderManager->LoadShader("SH_Ppfx", RESOURCE_DIR "/shaders/ppfx.wgsl");

  glm::vec2 screenSize = Application::Get()->GetWindowSize();
  uint32_t screenWidth = static_cast<uint32_t>(screenSize.x);
  uint32_t screenHeight = static_cast<uint32_t>(screenSize.y);

  // Shadow
  TextureProps shadowDepthTextureSpec = {};
  shadowDepthTextureSpec.Width = 4096;
  shadowDepthTextureSpec.Height = 4096;
  shadowDepthTextureSpec.Format = TextureFormat::Depth24Plus;
  shadowDepthTextureSpec.layers = m_NumOfCascades;
  // shadowDepthTextureSpec.layers = m_NumOfCascades;

  shadowDepthTextureSpec.DebugName = "ShadowMap";
  m_ShadowDepthTexture = Texture::Create(shadowDepthTextureSpec);

  // Common
  FramebufferSpec compositeFboSpec;
  compositeFboSpec.ColorFormat = TextureFormat::BRGBA8,
  compositeFboSpec.DepthFormat = TextureFormat::Depth24Plus;
  compositeFboSpec.DebugName = "FB_Composite";
  m_CompositeFramebuffer = Framebuffer::Create(compositeFboSpec);

  m_SceneUniformBuffer = GPUAllocator::GAlloc(WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, sizeof(SceneUniform));
  m_SceneUniformBuffer->SetData(&m_SceneUniform, sizeof(SceneUniform));

  m_CameraUniformBuffer = GPUAllocator::GAlloc(WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, sizeof(CameraData));
  m_CameraUniformBuffer->SetData(&m_CameraData, sizeof(CameraData));

  // Skybox
  FramebufferSpec skyboxFboSpec;
  skyboxFboSpec.ColorFormat = TextureFormat::BRGBA8,
  skyboxFboSpec.DepthFormat = TextureFormat::Depth24Plus;
  skyboxFboSpec.DebugName = "FB_Skybox";
  skyboxFboSpec.ExistingColorAttachment = m_CompositeFramebuffer->GetAttachment(0);

  RenderPipelineSpec skyboxPipeSpec = {
      .VertexLayout = vertexLayoutQuad,
      .InstanceLayout = {},
      .CullingMode = PipelineCullingMode::NONE,
      .Shader = skyboxShader,
      .TargetFramebuffer = Framebuffer::Create(skyboxFboSpec),
      .DebugName = "RP_Skybox"};

  m_SkyboxPipeline = RenderPipeline::Create(skyboxPipeSpec);
  auto skyboxSampler = Sampler::Create({.Name = "S_Skybox",
                                        .WrapFormat = TextureWrappingFormat::ClampToEdges,
                                        .MagFilterFormat = FilterMode::Linear,
                                        .MinFilterFormat = FilterMode::Linear,
                                        .MipFilterFormat = FilterMode::Linear,
                                        .Compare = CompareMode::CompareUndefined,
                                        .LodMinClamp = 0.0f,
                                        .LodMaxClamp = 1.0f});

  auto skybox = Rain::ResourceManager::LoadCubeTexture("T3D_Skybox",
                                                       {RESOURCE_DIR "/textures/right.jpg",
                                                        RESOURCE_DIR "/textures/left.jpg",
                                                        RESOURCE_DIR "/textures/top.jpg",
                                                        RESOURCE_DIR "/textures/bottom.jpg",
                                                        RESOURCE_DIR "/textures/front.jpg",
                                                        RESOURCE_DIR "/textures/back.jpg"});
  RenderPassSpec propSkyboxPass = {
      .Pipeline = m_SkyboxPipeline,
      .DebugName = "SykboxPass"};

  m_SkyboxPass = RenderPass::Create(propSkyboxPass);
  m_SkyboxPass->Set("cubemapTexture", skybox);
  m_SkyboxPass->Set("textureSampler", skyboxSampler);
  m_SkyboxPass->Set("u_Camera", m_CameraUniformBuffer);
  m_SkyboxPass->Bake();

  // Shadow

  m_ShadowUniformBuffer = GPUAllocator::GAlloc(WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform, sizeof(ShadowUniform));
  m_ShadowUniformBuffer->SetData(&m_ShadowUniform, sizeof(ShadowUniform));

  m_ShadowSampler = Sampler::Create({.Name = "ShadowSampler",
                                     .WrapFormat = TextureWrappingFormat::ClampToEdges,
                                     .MagFilterFormat = FilterMode::Linear,
                                     .MinFilterFormat = FilterMode::Nearest,
                                     .MipFilterFormat = FilterMode::Nearest,
                                     .Compare = CompareMode::Less,
                                     .LodMinClamp = 1.0f,
                                     .LodMaxClamp = 1.0f});

  FramebufferSpec shadowFboSpec;
  shadowFboSpec.DepthFormat = TextureFormat::Depth24Plus;
  shadowFboSpec.ExistingDepth = m_ShadowDepthTexture;

  RenderPipelineSpec shadowPipeSpec;
  shadowPipeSpec.VertexLayout = vertexLayout,
  shadowPipeSpec.InstanceLayout = instanceLayout,
  shadowPipeSpec.CullingMode = PipelineCullingMode::BACK,
  shadowPipeSpec.Shader = shadowShader;

  for (int i = 0; i < m_NumOfCascades; i++) {
    shadowFboSpec.DebugName = fmt::format("FB_Shadow_{}", i);

    shadowFboSpec.ExistingImageLayers.clear();
    shadowFboSpec.ExistingImageLayers.emplace_back(i);

    shadowPipeSpec.Overrides.clear();
    shadowPipeSpec.Overrides.emplace("co", i);

    shadowPipeSpec.DebugName = fmt::format("RP_Shadow_{}", i);
    shadowPipeSpec.TargetFramebuffer = Framebuffer::Create(shadowFboSpec);

    m_ShadowPipeline[i] = RenderPipeline::Create(shadowPipeSpec);

    RenderPassSpec propShadowPass = {
        .Pipeline = m_ShadowPipeline[i],
        .DebugName = "ShadowPass"};

    m_ShadowPass[i] = RenderPass::Create(propShadowPass);
    m_ShadowPass[i]->Set("u_ShadowData", m_ShadowUniformBuffer);
    m_ShadowPass[i]->Bake();
  }

  // Composite
  RenderPipelineSpec compositePipeSpec = {
      .VertexLayout = vertexLayout,
      .InstanceLayout = instanceLayout,
      .CullingMode = PipelineCullingMode::NONE,
      .Shader = pbrShader,
      .TargetFramebuffer = m_CompositeFramebuffer,
      .DebugName = "RP_Composite"};

  m_LitPipeline = RenderPipeline::Create(compositePipeSpec);

  RenderPassSpec litPassSpec = {
      .Pipeline = m_LitPipeline,
      .DebugName = "LitPass"};

  m_LitPass = RenderPass::Create(litPassSpec);
  m_LitPass->Set("u_Scene", m_SceneUniformBuffer);
  m_LitPass->Set("u_ShadowMap", m_ShadowPass[0]->GetDepthOutput());
  m_LitPass->Set("u_ShadowSampler", m_ShadowSampler);
  m_LitPass->Set("u_ShadowData", m_ShadowUniformBuffer);
  m_LitPass->Bake();

  // PPFX
  FramebufferSpec ppfxFboSpec;
  ppfxFboSpec.SwapChainTarget = true;
  ppfxFboSpec.ColorFormat = TextureFormat::BRGBA8;
  ppfxFboSpec.ExistingDepth = m_CompositeFramebuffer->GetDepthAttachment();
  ppfxFboSpec.DebugName = "FB_PostFX";

  RenderPipelineSpec ppfxPipeSpec = {
      .VertexLayout = vertexLayoutQuad,
      .InstanceLayout = {},
      .CullingMode = PipelineCullingMode::NONE,
      .Shader = ppfxShader,
      .TargetFramebuffer = Framebuffer::Create(ppfxFboSpec),
      .DebugName = "RP_PostFX"};

  m_PpfxPipeline = RenderPipeline::Create(ppfxPipeSpec);

  RenderPassSpec ppfxPassSpec = {.Pipeline = m_PpfxPipeline, .DebugName = "PpfxPass"};
  m_PpfxPass = RenderPass::Create(ppfxPassSpec);
  m_PpfxPass->Set("renderTexture", m_LitPass->GetOutput(0));
  m_PpfxPass->Set("textureSampler", Render::GetDefaultSampler());
  m_PpfxPass->Bake();

  auto renderContext = Render::Instance->GetRenderContext();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  auto& io = ImGui::GetIO();
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

void SceneRenderer::SetViewportSize(int height, int width) {
  m_ViewportWidth = width;
  m_ViewportHeight = height;

  m_NeedResize = true;
}

void SceneRenderer::BeginScene(const SceneCamera& camera) {
  RN_PROFILE_FUNC;
  const glm::mat4 viewInverse = glm::inverse(camera.ViewMatrix);
  const glm::vec3 cameraPosition = viewInverse[3];

  m_SceneUniform.ViewProjection = camera.Projection * camera.ViewMatrix;
  m_SceneUniform.View = camera.ViewMatrix;
	m_SceneUniform.LightDir = m_Scene->SceneLightInfo.LightDirection;

  m_SceneUniform.CameraPosition = cameraPosition;

  glm::mat4 skyboxViewMatrix = camera.ViewMatrix;
  skyboxViewMatrix[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

  m_CameraData.InverseViewProjectionMatrix = glm::inverse(skyboxViewMatrix) * glm::inverse(camera.Projection);

  CascadeData data[4];
  CalculateCascades(data, camera, m_Scene->SceneLightInfo.LightDirection);
  m_ShadowUniform.ShadowViews[0] = data[0].ViewProj;
  m_ShadowUniform.ShadowViews[1] = data[1].ViewProj;
  m_ShadowUniform.ShadowViews[2] = data[2].ViewProj;
  m_ShadowUniform.ShadowViews[3] = data[3].ViewProj;

  m_ShadowUniform.CascadeDistances = glm::vec4(data[0].SplitDepth, data[1].SplitDepth, data[2].SplitDepth, data[3].SplitDepth);

  m_SceneUniformBuffer->SetData(&m_SceneUniform, sizeof(SceneUniform));
  m_CameraUniformBuffer->SetData(&m_CameraData, sizeof(CameraData));
  m_ShadowUniformBuffer->SetData(&m_ShadowUniform, sizeof(ShadowUniform));

  if (m_NeedResize) {
    m_SkyboxPass->GetTargetFrameBuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
    m_LitPass->GetTargetFrameBuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
    m_PpfxPass->GetTargetFrameBuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
    m_NeedResize = false;
  }

  ImGui_ImplWGPU_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void SceneRenderer::FlushDrawList() {
  RN_PROFILE_FUNC;
  WGPUCommandEncoderDescriptor commandEncoderDesc = {};
	ZERO_INIT(commandEncoderDesc);

	commandEncoderDesc.label = "Default";
	commandEncoderDesc.nextInChain = nullptr;
  Ref<RenderContext> renderContext = Render::Instance->GetRenderContext();

  auto commandEncoder = wgpuDeviceCreateCommandEncoder(renderContext->GetDevice(), &commandEncoderDesc);

  {
    RN_PROFILE_FUNCN("Skybox Pass");
    auto skyboxPassEncoder = Render::BeginRenderPass(m_SkyboxPass, commandEncoder);
    Render::Instance->SubmitFullscreenQuad(skyboxPassEncoder, m_SkyboxPipeline->GetPipeline());
    Render::EndRenderPass(m_SkyboxPass, skyboxPassEncoder);
  }

  {
    RN_PROFILE_FUNCN("Shadow Pass");

    for (int i = 0; i < m_NumOfCascades; i++) {
      auto shadowPassEncoder = Render::BeginRenderPass(m_ShadowPass[i], commandEncoder);
      for (auto& [mk, dc] : m_DrawList) {
        Render::Instance->RenderMesh(shadowPassEncoder, m_ShadowPipeline[i]->GetPipeline(), dc.Mesh, dc.SubmeshIndex, dc.Materials, m_TransformBuffer, m_MeshTransformMap[mk].TransformOffset, dc.InstanceCount);
      }
      Render::EndRenderPass(m_ShadowPass[i], shadowPassEncoder);
    }
  }

  {
    RN_PROFILE_FUNCN("Geometry Pass");
    auto litPassEncoder = Render::BeginRenderPass(m_LitPass, commandEncoder);
    for (auto& [mk, dc] : m_DrawList) {
      Render::Instance->RenderMesh(litPassEncoder, m_LitPipeline->GetPipeline(), dc.Mesh, dc.SubmeshIndex, dc.Materials, m_TransformBuffer, m_MeshTransformMap[mk].TransformOffset, dc.InstanceCount);
    }
    Render::EndRenderPass(m_LitPass, litPassEncoder);
  }

  {
    RN_PROFILE_FUNCN("Post FX Pass");
    auto ppfxPassEncoder = Render::BeginRenderPass(m_PpfxPass, commandEncoder);
    Render::Instance->SubmitFullscreenQuad(ppfxPassEncoder, m_PpfxPipeline->GetPipeline());
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), ppfxPassEncoder);
    Render::EndRenderPass(m_PpfxPass, ppfxPassEncoder);
  }

  {
    RN_PROFILE_FUNCN("Submit");
    WGPUCommandBufferDescriptor cmdBufferDescriptor = {.nextInChain = nullptr, .label = "Command Buffer"};
    WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(commandEncoder, &cmdBufferDescriptor);
    wgpuQueueSubmit(*renderContext->GetQueue(), 1, &commandBuffer);

    wgpuCommandBufferRelease(commandBuffer);
    wgpuCommandEncoderRelease(commandEncoder);

#ifndef __EMSCRIPTEN__
    wgpuSwapChainPresent(Render::Instance->m_swapChain);
    wgpuDeviceTick(renderContext->GetDevice());
#endif
  }

  m_DrawList.clear();
  m_MeshTransformMap.clear();
}

void SceneRenderer::EndScene() {
  ImGui::EndFrame();
  ImGui::Render();
  PreRender();
  FlushDrawList();
}
