#include <Jolt.h>
#include <Jolt/Renderer/DebugRendererSimple.h>
#include <webgpu/webgpu.h>
#include <glm/glm.hpp>
#include "render/GPUAllocator.h"
#include "render/Shader.h"

namespace Rain {
  struct LineVertex {
    glm::vec3 position;
    glm::vec4 color;
  };
  struct QueuedLine {
    glm::vec3 p0;
    glm::vec3 p1;
    glm::vec4 color;
  };

  struct LineUniforms {
    glm::mat4 viewProjectionMatrix;
  };

  class RenderDebug : public JPH::DebugRendererSimple {
   public:
    static void Init();
    void Shutdown();
    static void Begin(WGPURenderPassEncoder* pass, WGPUCommandEncoder* encoder);
    static void SetMVP(glm::mat4 mvp) {
      Instance->m_MVP = mvp;
    }

    static void DrawCircle(const glm::vec3& p0, const glm::vec3& rotation, float radius, const glm::vec4& color);
    static void DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color);

    virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;
    virtual void DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor, float inHeight) override;
    virtual void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow) override;

    static void FlushDrawList();

   private:
    void InitializeResources();

    static std::unique_ptr<RenderDebug> Instance;
    WGPURenderPassEncoder* m_TargetPass;
    WGPUCommandEncoder* m_TargetEncoder;
    bool init = false;

    Ref<Shader> m_LineShader;
    WGPUBindGroup m_LineBindGroup = nullptr;
    WGPURenderPipeline m_LinePipeline;
    glm::mat4 m_MVP;
    Ref<GPUBuffer> m_LineVertexBuffer = nullptr;
    Ref<GPUBuffer> m_LineUniformBuffer = nullptr;
    std::vector<QueuedLine> m_QueuedLines;
    static constexpr size_t MAX_LINES_PER_BATCH = 1000;
  };
}  // namespace Rain
