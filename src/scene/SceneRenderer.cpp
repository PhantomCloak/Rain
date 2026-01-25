#include "SceneRenderer.h"
#include <glm/glm.hpp>
#include <memory>
#include "Application.h"
#include "core/Log.h"

#ifndef __EMSCRIPTEN__
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include "imgui.h"
#endif

#include "debug/Profiler.h"
#include "io/filesystem.h"
#include "io/keyboard.h"
#include "render/CommandEncoder.h"
#include "render/Framebuffer.h"
#include "render/Render.h"
#include "render/ResourceManager.h"
#include "render/ShaderManager.h"

namespace Rain
{
  SceneRenderer* SceneRenderer::instance;

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
    // float CascadeFarPlaneOffset = 350.0f, CascadeNearPlaneOffset = -350.0f;
    // float CascadeFarPlaneOffset = 320.0f, CascadeNearPlaneOffset = -320.0f;
    float CascadeFarPlaneOffset = 250.0f, CascadeNearPlaneOffset = -250.0f;
    // float CascadeFarPlaneOffset = 350.0f, CascadeNearPlaneOffset = -350.0f;
    // float CascadeFarPlaneOffset = 50.0f, CascadeNearPlaneOffset = -50.0f;
    // float CascadeFarPlaneOffset = 50.0f, CascadeNearPlaneOffset = -50.0f;
    // float CascadeFarPlaneOffset = 320.0f, CascadeNearPlaneOffset = -100.0f;

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
    // cascadeSplits[0] = 0.02f;
    // cascadeSplits[1] = 0.15f;
    // cascadeSplits[2] = 0.3f;
    // cascadeSplits[3] = 1.0f;

    // Calculate orthographic projection matrix for each cascade
    float lastSplitDist = 0.0;
    for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
    {
      float splitDist = cascadeSplits[i];

      glm::vec3 frustumCorners[8] =
          {
              glm::vec3(-1.0f, 1.0f, -1.0f),
              glm::vec3(1.0f, 1.0f, -1.0f),
              glm::vec3(1.0f, -1.0f, -1.0f),
              glm::vec3(-1.0f, -1.0f, -1.0f),
              glm::vec3(-1.0f, 1.0f, 1.0f),
              glm::vec3(1.0f, 1.0f, 1.0f),
              glm::vec3(1.0f, -1.0f, 1.0f),
              glm::vec3(-1.0f, -1.0f, 1.0f),
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
      {
        frustumCenter += frustumCorners[i];
      }

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

      glm::mat4 lightViewMatrix = glm::lookAt(
          frustumCenter - lightDir * radius,
          frustumCenter,
          glm::vec3(0.0f, 1.0f, 0.0f));

      glm::mat4 lightOrthoMatrix = glm::ortho(
          minExtents.x, maxExtents.x,
          minExtents.y, maxExtents.y,
          minExtents.z + CascadeNearPlaneOffset,
          maxExtents.z + CascadeFarPlaneOffset);

      glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
      float ShadowMapResolution = 4096;

      glm::vec4 shadowOrigin = (shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) * ShadowMapResolution / 2.0f;
      glm::vec4 roundedOrigin = glm::round(shadowOrigin);
      glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
      roundOffset = roundOffset * 2.0f / ShadowMapResolution;
      roundOffset.z = 0.0f;
      roundOffset.w = 0.0f;

      lightOrthoMatrix[3] += roundOffset;

      cascades[i].SplitDepth = nearClip + splitDist * clipRange;
      cascades[i].ViewProj = lightOrthoMatrix * lightViewMatrix;
      cascades[i].View = lightViewMatrix;

      lastSplitDist = cascadeSplits[i];
    }
  }

  void SceneRenderer::SubmitMesh(Ref<MeshSource> meshSource, uint32_t submeshIndex, Ref<MaterialTable> materialTable, glm::mat4& transform)
  {
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
  std::vector<std::function<void(std::string fileName)>> callbacks;

  void SceneRenderer::Init()
  {
    if (const auto ptr = Render::Get())
    {
    }
    else
    {
      return;
    }

    m_TransformBuffer = GPUAllocator::GAlloc("scene_global_transform", WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex, 1024 * sizeof(TransformVertexData));
    m_SceneUniform = {};

    // clang-format off
	VertexBufferLayout vertexLayout = {80, {
			{0, ShaderDataType::Float3, "position", 0},
			{1, ShaderDataType::Float3, "normal", 16},
			{2, ShaderDataType::Float2, "uv", 32},
			{3, ShaderDataType::Float3, "tangent", 48},
			{4, ShaderDataType::Float3, "bitangent", 60}}};

	VertexBufferLayout vertexLayoutQuad = {32, {
			{0, ShaderDataType::Float3, "position", 0},
			{1, ShaderDataType::Float2, "uv", 16}}};

  VertexBufferLayout instanceLayout = {48, {
      {5, ShaderDataType::Float4, "a_MRow0", 0},
      {6, ShaderDataType::Float4, "a_MRow1", 16},
      {7, ShaderDataType::Float4, "a_MRow2", 32}}};
    // clang-format on

    auto pbrShader = ShaderManager::LoadShader("SH_DefaultBasicBatch", RESOURCE_DIR "/shaders/pbr.wgsl");
    auto shadowShader = ShaderManager::LoadShader("SH_Shadow", RESOURCE_DIR "/shaders/shadow_map.wgsl");
    auto skyboxShader = ShaderManager::LoadShader("SH_Skybox", RESOURCE_DIR "/shaders/skybox.wgsl");
    auto ppfxShader = ShaderManager::LoadShader("SH_Ppfx", RESOURCE_DIR "/shaders/ppfx.wgsl");

    TextureProps cubeProps = {};
    cubeProps.Width = 2048;
    cubeProps.Height = 2048;
    cubeProps.GenerateMips = true;
    cubeProps.Format = TextureFormat::RGBA16F;

    Ref<TextureCube> envUnfiltered = TextureCube::Create(cubeProps);
    Ref<Texture2D> envEquirect = Texture2D::Create(TextureProps(), RESOURCE_DIR "/textures/evening_road_01_puresky_4k.hdr");

    m_Renderer = Render::Get();
    m_Renderer->ComputeEquirectToCubemap(envEquirect.get(), envUnfiltered.get());

    auto [envFiltered, envIrradiance] = m_Renderer->CreateEnvironmentMap(RESOURCE_DIR "/textures/evening_road_01_puresky_4k.hdr");

    FileSys::WatchFile(RESOURCE_DIR "/shaders/pbr.wgsl", [pbrShader](std::string filePath)
                       {
                         std::string content = FileSys::ReadFile(filePath);
                         pbrShader->Reload(content);
                         Render::ReloadShader(pbrShader);

                         RN_LOG("Shader {} reloaded", filePath); });

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
    m_ShadowDepthTexture = Texture2D::Create(shadowDepthTextureSpec);

    // Common
    FramebufferSpec compositeFboSpec;
    compositeFboSpec.ColorFormat = TextureFormat::BRGBA8,
    compositeFboSpec.DepthFormat = TextureFormat::Depth24Plus;
    compositeFboSpec.DebugName = "FB_Composite";
    compositeFboSpec.Multisample = 4;
    compositeFboSpec.SwapChainTarget = true;
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
    skyboxFboSpec.Multisample = 4;
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
                                          .LodMaxClamp = 12.0f});
    auto skyboxSampler2 = Sampler::Create({.Name = "S_Skybox2",
                                           .WrapFormat = TextureWrappingFormat::ClampToEdges,
                                           .MagFilterFormat = FilterMode::Linear,
                                           .MinFilterFormat = FilterMode::Linear,
                                           .MipFilterFormat = FilterMode::Linear,
                                           .Compare = CompareMode::CompareUndefined,
                                           .LodMinClamp = 0.0f,
                                           .LodMaxClamp = 1.0f});

    auto bdrfLut = Rain::ResourceManager::LoadTexture("BDRF", RESOURCE_DIR "/textures/BRDF_LUT.png");

    RenderPassSpec propSkyboxPass = {
        .Pipeline = m_SkyboxPipeline,
        .DebugName = "SykboxPass"};

    m_SkyboxPass = RenderPass::Create(propSkyboxPass);
    m_SkyboxPass->Set("u_Texture", envUnfiltered);
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

    for (int i = 0; i < m_NumOfCascades; i++)
    {
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
    m_LitPass->Set("u_radianceMap", envFiltered);
    m_LitPass->Set("u_irradianceMap", envIrradiance);
    m_LitPass->Set("u_radianceMapSampler", skyboxSampler);
    m_LitPass->Set("u_irradianceMapSampler", skyboxSampler2);
    m_LitPass->Set("u_BDRFLut", bdrfLut);
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

    // auto renderContext = m_Renderer->GetRenderContext();

#ifndef __EMSCRIPTEN__
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(2.0f);  // Scale everything by 2x
    io.FontGlobalScale = 2.0f;
    ImGui_ImplGlfw_InitForOther((GLFWwindow*)Application::Get()->GetNativeWindow(), true);

    ImGui_ImplWGPU_InitInfo initInfo;
    initInfo.Device = RenderContext::GetDevice();
    // initInfo.RenderTargetFormat = render->m_swapChainFormat;
    initInfo.RenderTargetFormat = WGPUTextureFormat_BGRA8Unorm;
    initInfo.PipelineMultisampleState = {
        .nextInChain = nullptr,
        .count = 4,
        .mask = ~0u,
        .alphaToCoverageEnabled = false};
    initInfo.DepthStencilFormat = WGPUTextureFormat_Depth24Plus;
    ImGui_ImplWGPU_Init(&initInfo);
#endif
  }

  void SceneRenderer::PreRender()
  {
    RN_PROFILE_FUNC;
    static TransformVertexData* submeshTransforms = (TransformVertexData*)malloc(1024 * sizeof(TransformVertexData));

    uint32_t offset = 0;
    for (auto& [key, transformData] : m_MeshTransformMap)
    {
      transformData.TransformOffset = offset * sizeof(TransformVertexData);
      for (const auto& transform : transformData.Transforms)
      {
        submeshTransforms[offset] = (transform);
        offset++;
      }
    }

    m_TransformBuffer->SetData(submeshTransforms, offset * sizeof(TransformVertexData));
  }

  void SceneRenderer::SetScene(Scene* scene)
  {
    RN_ASSERT(scene != nullptr, "Scene cannot be null");
    m_Scene = scene;
  }

  void SceneRenderer::SetViewportSize(int height, int width)
  {
    m_ViewportWidth = width;
    m_ViewportHeight = height;

    m_NeedResize = true;
  }

  void DrawCameraFrustum(SceneCamera camera)
  {
    // Get the inverse of the view-projection matrix to transform from NDC to world space
    glm::mat4 invViewProj = glm::inverse(camera.Projection * camera.ViewMatrix);

    // Define the 8 corners of the frustum in NDC space (normalized device coordinates)
    // Near plane corners
    glm::vec4 nearCorners[4] = {
        glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f),  // Bottom-left near
        glm::vec4(1.0f, -1.0f, -1.0f, 1.0f),   // Bottom-right near
        glm::vec4(1.0f, 1.0f, -1.0f, 1.0f),    // Top-right near
        glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f)    // Top-left near
    };

    // Far plane corners
    glm::vec4 farCorners[4] = {
        glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f),  // Bottom-left far
        glm::vec4(1.0f, -1.0f, 1.0f, 1.0f),   // Bottom-right far
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),    // Top-right far
        glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f)    // Top-left far
    };

    // Transform corners to world space
    JPH::RVec3 nearWorldCorners[4];
    JPH::RVec3 farWorldCorners[4];

    for (int i = 0; i < 4; i++)
    {
      // Transform near corners
      glm::vec4 nearWorld = invViewProj * nearCorners[i];
      nearWorld /= nearWorld.w;  // Perspective divide
      nearWorldCorners[i] = JPH::RVec3(nearWorld.x, nearWorld.y, nearWorld.z);

      // Transform far corners
      glm::vec4 farWorld = invViewProj * farCorners[i];
      farWorld /= farWorld.w;  // Perspective divide
      farWorldCorners[i] = JPH::RVec3(farWorld.x, farWorld.y, farWorld.z);
    }

    // Draw the frustum lines
    // Near plane edges
    for (int i = 0; i < 4; i++)
    {
      int next = (i + 1) % 4;
      RenderDebug::sInstance->DrawLine(nearWorldCorners[i], nearWorldCorners[next], JPH::Color::sGreen);
    }

    // Far plane edges
    for (int i = 0; i < 4; i++)
    {
      int next = (i + 1) % 4;
      RenderDebug::sInstance->DrawLine(farWorldCorners[i], farWorldCorners[next], JPH::Color::sGreen);
    }

    // Connecting edges (from near to far)
    for (int i = 0; i < 4; i++)
    {
      RenderDebug::sInstance->DrawLine(nearWorldCorners[i], farWorldCorners[i], JPH::Color::sGreen);
    }
  }

  void SceneRenderer::BeginScene(const SceneCamera& camera)
  {
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
    if (m_NeedResize)
    {
      m_SkyboxPass->GetTargetFrameBuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
      m_LitPass->GetTargetFrameBuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
      // m_PpfxPass->GetTargetFrameBuffer()->Resize(m_ViewportWidth, m_ViewportHeight);
      m_NeedResize = false;
    }

    // JPH::DebugRenderer::sInstance = m_Renderer;
#ifndef __EMSCRIPTEN__
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
#endif

    if (SavedCam.Far != 400.0f)
    {
      SavedCam = camera;
    }
  }

  void SceneRenderer::FlushDrawList()
  {
    RN_PROFILE_FUNC;
    if (Keyboard::IsKeyPressed(Key::F))
    {
      SavedCam = Cam;
    }

    const auto commandEncoder = m_Renderer->CreateCommandEncoder();
    {
      RN_PROFILE_FUNCN("Skybox Pass");
      Ref<RenderPassEncoder> skyboxPassEncoder = m_Renderer->BeginRenderPass(m_SkyboxPass, commandEncoder);
      m_Renderer->SubmitFullscreenQuad(skyboxPassEncoder, m_SkyboxPipeline->GetPipeline());
      m_Renderer->EndRenderPass(m_SkyboxPass, skyboxPassEncoder);
    }

    {
      RN_PROFILE_FUNCN("Shadow Pass");

      for (int i = 0; i < m_NumOfCascades; i++)
      {
        Ref<RenderPassEncoder> shadowPassEncoder = m_Renderer->BeginRenderPass(m_ShadowPass[i], commandEncoder);
        for (auto& [mk, dc] : m_DrawList)
        {
          m_Renderer->RenderMesh(shadowPassEncoder, m_ShadowPipeline[i]->GetPipeline(), dc.Mesh, dc.SubmeshIndex, dc.Materials, m_TransformBuffer, m_MeshTransformMap[mk].TransformOffset, dc.InstanceCount);
        }
        m_Renderer->EndRenderPass(m_ShadowPass[i], shadowPassEncoder);
      }
    }

    {
      RN_PROFILE_FUNCN("Geometry Pass");
      Ref<RenderPassEncoder> litPassEncoder = m_Renderer->BeginRenderPass(m_LitPass, commandEncoder);
      for (auto& [mk, dc] : m_DrawList)
      {
        m_Renderer->RenderMesh(litPassEncoder, m_LitPipeline->GetPipeline(), dc.Mesh, dc.SubmeshIndex, dc.Materials, m_TransformBuffer, m_MeshTransformMap[mk].TransformOffset, dc.InstanceCount);
      }

      // RenderDebug::Begin(&litPassEncoder, &commandEncoder);
      // RenderDebug::SetMVP(m_SceneUniform.ViewProjection);
      // DrawCameraFrustum(SavedCam);
      //  RenderDebug::FlushDrawList();

#ifndef __EMSCRIPTEN__
      ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), std::static_pointer_cast<RenderPassEncoderWGPU>(litPassEncoder)->GetNativeEncoder());
#endif

      m_Renderer->EndRenderPass(m_LitPass, litPassEncoder);
    }

    m_Renderer->SubmitAndFinishEncoder(commandEncoder);
    m_Renderer->Present();

    m_DrawList.clear();
    m_MeshTransformMap.clear();
  }

  void SceneRenderer::EndScene()
  {
    const auto renderer = Render::Get();
    if (renderer == nullptr)
    {
      return;
    }

#ifndef __EMSCRIPTEN__
    ImGui::EndFrame();
    ImGui::Render();
#endif

    PreRender();
    FlushDrawList();
  }
}  // namespace Rain
