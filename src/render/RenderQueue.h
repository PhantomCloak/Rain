#pragma once
#include "render/Model.h"

#define PRE_ALLOCATED_BATCH_GROUPS 2
#define RENDER_BATCH_SIZE 256

struct RenderQueueItem {
  glm::mat4 ModelMatrix;
  int MeshSourceId;
  uint32_t NodeIdx;
  MeshNode Node;
};

struct MeshKey {
  UUID MeshHandle;
  UUID MaterialHandle;
  uint32_t SubmeshIndex;
};

class RenderQueue {
 public:
  static void Init();
  static void AddQueue(RenderQueueItem item);
  static void Clear();
  static void DrawEntities(WGPURenderPassEncoder& renderPass, WGPURenderPipeline& pipeline);
  static void IndexMeshSource(Ref<MeshSource> meshSource);

 private:
  static std::vector<uint32_t> offsetArray;
};
