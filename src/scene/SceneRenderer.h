#pragma once
#include "Cam.h"
#include "Scene.h"
#include "render/Pipeline.h"
#include "render/RenderPass.h"
#include "render/ShaderManager.h"

struct MeshKey {
  UUID MeshHandle;
  UUID MaterialHandle;
  uint32_t SubmeshIndex;

  MeshKey(UUID meshHandle, UUID materialHandle, uint32_t submeshIndex)
      : MeshHandle(meshHandle), MaterialHandle(materialHandle), SubmeshIndex(submeshIndex) {
  }

  bool operator<(const MeshKey& other) const {
    if (MeshHandle < other.MeshHandle) {
      return true;
    }

    if (MeshHandle > other.MeshHandle) {
      return false;
    }

    if (SubmeshIndex < other.SubmeshIndex) {
      return true;
    }

    if (SubmeshIndex > other.SubmeshIndex) {
      return false;
    }

    if (MaterialHandle < other.MaterialHandle) {
      return true;
    }

    if (MaterialHandle > other.MaterialHandle) {
      return false;
    }

    return false;
  }
};

struct DrawCommand {
  Ref<MeshSource> Mesh;
  uint32_t SubmeshIndex;
  uint32_t MaterialIndex;

  uint32_t InstanceCount = 0;
  uint32_t InstanceOffset = 0;
  bool IsRigged = false;
};

struct TransformVertexData {
  glm::vec4 MRow[3];
};

struct TransformMapData {
  std::vector<TransformVertexData> Transforms;
  uint32_t TransformOffset = 0;
};

struct SceneCamera {
  glm::mat4 ViewMatrix;
  glm::mat4 Projection;
  float Near;
  float Far;
};

struct SceneUniform {
  glm::mat4x4 viewProjection;
  glm::mat4x4 shadowViewProjection;
  glm::mat4x4 cameraViewMatrix;
  glm::vec3 lightPos;
  float _pad;
};

class SceneRenderer {
 public:
  SceneRenderer()
      : m_ShaderManager(CreateRef<ShaderManager>()) { instance = this; };

  void Init();
  void SubmitMesh(Ref<MeshSource> meshSource, uint32_t submeshIndex, uint32_t materialIndex, glm::mat4& transform);
  void BeginScene(const SceneCamera& camera);
  void EndScene();
  void SetScene(Scene* scene);
  static SceneRenderer* instance;
  Ref<ShaderManager> m_ShaderManager;

 private:
  void PreRender();
  void FlushDrawList();

 private:
  Scene* m_Scene;
  Ref<GPUBuffer> m_TransformBuffer;
  Ref<GPUBuffer> m_SceneUniformBuffer;
  SceneUniform m_SceneUniform;
  std::map<MeshKey, DrawCommand> m_DrawList;
  std::map<MeshKey, TransformMapData> m_MeshTransformMap;

  Ref<Texture> m_ShadowDepthTexture;
  Ref<Texture> m_LitDepthTexture;
  Ref<Texture> m_LitPassTexture;

  Ref<Sampler> m_ShadowSampler;
  Ref<Sampler> m_PpfxSampler;

	Ref<RenderPass> m_ShadowPass;
	Ref<RenderPass> m_LitPass;
	Ref<RenderPass> m_PpfxPass;

  Ref<RenderPipeline> m_LitPipeline;
  Ref<RenderPipeline> m_ShadowPipeline;
  Ref<RenderPipeline> m_DebugPipeline;
  Ref<RenderPipeline> m_PpfxPipeline;
};
