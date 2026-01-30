#pragma once
#include "Scene.h"
#include "animation/OzzAnimator.h"
#include "render/CommandBuffer.h"
#include "render/Pipeline.h"
#include "render/PipelineCompute.h"
#include "render/Render.h"
#include "render/Render2D.h"
#include "render/RenderPass.h"

namespace Rain
{
  struct MeshKey
  {
    UUID MeshHandle;
    UUID MaterialHandle;
    uint32_t SubmeshIndex;

    MeshKey(UUID meshHandle, UUID materialHandle, uint32_t submeshIndex)
        : MeshHandle(meshHandle), MaterialHandle(materialHandle), SubmeshIndex(submeshIndex)
    {
    }

    bool operator<(const MeshKey& other) const
    {
      if (MeshHandle < other.MeshHandle)
      {
        return true;
      }

      if (MeshHandle > other.MeshHandle)
      {
        return false;
      }

      if (SubmeshIndex < other.SubmeshIndex)
      {
        return true;
      }

      if (SubmeshIndex > other.SubmeshIndex)
      {
        return false;
      }

      if (MaterialHandle < other.MaterialHandle)
      {
        return true;
      }

      if (MaterialHandle > other.MaterialHandle)
      {
        return false;
      }

      return false;
    }
  };

  struct DrawCommand
  {
    Ref<MeshSource> Mesh;
    uint32_t SubmeshIndex;
    Ref<MaterialTable> Materials;

    uint32_t InstanceCount = 0;
    uint32_t InstanceOffset = 0;
    bool IsSkeletal = false;
  };

  struct SkeletalDrawCommand
  {
    Ref<MeshSource> Mesh;
    uint32_t SubmeshIndex;
    Ref<MaterialTable> Materials;
    glm::mat4 Transform;
    Ref<OzzAnimator> Animator;
  };

  struct TransformVertexData
  {
    glm::vec4 MRow[3];
  };

  struct TransformMapData
  {
    std::vector<TransformVertexData> Transforms;
    uint32_t TransformOffset = 0;
  };

  struct SceneCamera
  {
    glm::mat4 ViewMatrix;
    glm::mat4 Projection;
    float Near;
    float Far;
  };

  struct CameraData
  {
    glm::mat4 InverseViewProjectionMatrix;
  };

  struct SceneUniform
  {
    glm::mat4x4 ViewProjection;
    glm::mat4x4 View;
    glm::vec3 CameraPosition;
    float _pad0;
    glm::vec3 LightDir;
    float _pad1;
  };

  struct ShadowUniform
  {
    glm::mat4 ShadowViews[4];
    glm::vec4 CascadeDistances;
  };

  class SceneRenderer
  {
   public:
    SceneRenderer() { instance = this; };

    void Init();
    void SubmitMesh(Ref<MeshSource> meshSource, uint32_t submeshIndex, Ref<MaterialTable> materialTable, glm::mat4& transform, Ref<OzzAnimator> animator = nullptr);
    void SubmitSkeletalMesh(Ref<MeshSource> meshSource, uint32_t submeshIndex, Ref<MaterialTable> materialTable, glm::mat4& transform, Ref<OzzAnimator> animator = nullptr);
    void BeginScene(const SceneCamera& camera);
    void EndScene();
    void SetScene(Scene* scene);
    void SetViewportSize(int height, int width);
    Ref<Texture2D> GetLastPassImage();

    static SceneRenderer* instance;

   private:
    void PreRender();
    void FlushDrawList();

   private:
    Scene* m_Scene = nullptr;
    Render* m_Renderer = nullptr;

    CameraData m_CameraData;
    Ref<GPUBuffer> m_CameraUniformBuffer;

    Ref<GPUBuffer> m_TransformBuffer;
    Ref<GPUBuffer> m_SceneUniformBuffer;
    Ref<GPUBuffer> m_ShadowUniformBuffer;

    SceneUniform m_SceneUniform;
    ShadowUniform m_ShadowUniform;

    std::map<MeshKey, DrawCommand> m_DrawList;
    std::map<MeshKey, TransformMapData> m_MeshTransformMap;

    Ref<Texture2D> m_ShadowDepthTexture;
    Ref<Texture2D> m_LitDepthTexture;
    Ref<Texture2D> m_LitPassTexture;
    SceneCamera Cam;
    SceneCamera SavedCam;

    Ref<Sampler> m_ShadowSampler;

    Ref<Framebuffer> m_CompositeFramebuffer;

    Ref<RenderPass> m_ShadowPass[4];
    Ref<RenderPass> m_LitPass;
    Ref<RenderPass> m_VoxelPass;
    Ref<RenderPass> m_PpfxPass;
    Ref<RenderPass> m_SkyboxPass;

    // Ref<RenderPipeline> m_ShadowPipeline;
    Ref<RenderPipeline> m_ShadowPipeline[4];
    Ref<RenderPipeline> m_LitPipeline;
    Ref<RenderPipeline> m_DebugPipeline;
    Ref<RenderPipeline> m_PpfxPipeline;
    Ref<RenderPipeline> m_SkyboxPipeline;

    Ref<TextureCube> m_RadianceMap;
    Ref<TextureCube> m_IrradianceMap;

    bool m_NeedResize = false;
    uint32_t m_NumOfCascades = 4;

    uint32_t m_ViewportWidth;
    uint32_t m_ViewportHeight;

    Ref<CommandBuffer> m_CommandBuffer;

    // Skeletal rendering
    Ref<RenderPipeline> m_SkeletalPipeline;
    Ref<RenderPass> m_SkeletalPass;
    Ref<GPUBuffer> m_BoneMatricesBuffer;
    Ref<GPUBuffer> m_SkeletalTransformBuffer;
    std::vector<SkeletalDrawCommand> m_SkeletalDrawList;

    void RenderSkeletalMeshes(Ref<RenderPass> passEncoder);
  };
}  // namespace Rain
