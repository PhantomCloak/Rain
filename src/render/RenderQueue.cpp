#include "RenderQueue.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include "render/GPUAllocator.h"
#include "render/Render.h"
#include "render/RenderUtils.h"

struct BatchGroup {
  WGPUBindGroup bindGroup;
  GPUBuffer buffer;
  int offset;
};

std::vector<BatchGroup> _batchGroups;
std::unordered_map<int, std::vector<int>> materialSubmeshes;
std::unordered_map<int, std::vector<Ref<MeshNode>>> subMeshNodes;
std::unordered_map<int, Ref<MeshSource>> indexedMeshSources;

Ref<MeshSource> meshSource = nullptr;

void IndexMeshSource(Ref<MeshSource> meshSource) {

}

void RenderQueue::AddQueue(GameObject* gameObject) {
  if (meshSource == nullptr) {
    meshSource = gameObject->modelExp;

    for (int i = 0; i < meshSource->m_SubMeshes.size(); i++) {
      const SubMesh& subMesh = meshSource->m_SubMeshes[i];
      materialSubmeshes[subMesh.MaterialIndex].push_back(i);
    }

    for (const auto& node : gameObject->modelExp->m_Nodes) {
			subMeshNodes[node->SubMeshId].push_back(node);
    }
  }
}

BatchGroup CreateBindBatch2(WGPUBindGroupLayout layout) {
  static int nextBindgroupId = 0;

  BatchGroup result;

  result.buffer = GPUAllocator::GAlloc(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, sizeof(SceneUniform) * RENDER_BATCH_SIZE);
  std::vector<WGPUBindGroupEntry> binding(1);

  binding[0].binding = 0;
  binding[0].buffer = result.buffer.Buffer;
  binding[0].offset = 0;
  binding[0].size = sizeof(SceneUniform) * RENDER_BATCH_SIZE;

  std::string bgLabel = "bgl_scene_batch:" + std::to_string(nextBindgroupId++);

  WGPUBindGroupDescriptor bgSceneDesc = {.label = bgLabel.c_str()};
  bgSceneDesc.layout = layout;
  bgSceneDesc.entryCount = (uint32_t)binding.size();
  bgSceneDesc.entries = binding.data();

  result.bindGroup = wgpuDeviceCreateBindGroup(Render::Instance->m_device, &bgSceneDesc);

  return result;
}

BatchGroup* bgTransform;
void RenderQueue::Init() {
  static GroupLayout sceneGroup = {{0, GroupLayoutVisibility::Vertex, GroupLayoutType::StorageReadOnly}};
  static WGPUBindGroupLayout sceneLayout = LayoutUtils::CreateBindGroup("bgl_gameobject_scene", Render::Instance->m_device, sceneGroup);

  static int nextBindgroupId = 0;

  bgTransform = new BatchGroup();

  bgTransform->buffer = GPUAllocator::GAlloc(WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, sizeof(SceneUniform) * RENDER_BATCH_SIZE);
  std::vector<WGPUBindGroupEntry> binding(1);

  binding[0].binding = 0;
  binding[0].buffer = bgTransform->buffer.Buffer;
  binding[0].offset = 0;
  binding[0].size = sizeof(SceneUniform) * RENDER_BATCH_SIZE;

  std::string bgLabel = "bgl_scene_batch:" + std::to_string(nextBindgroupId++);

  WGPUBindGroupDescriptor bgSceneDesc = {.label = bgLabel.c_str()};
  bgSceneDesc.layout = sceneLayout;
  bgSceneDesc.entryCount = (uint32_t)binding.size();
  bgSceneDesc.entries = binding.data();

  bgTransform->bindGroup = wgpuDeviceCreateBindGroup(Render::Instance->m_device, &bgSceneDesc);
}

void RenderQueue::DrawEntities(WGPURenderPassEncoder& renderPass, WGPURenderPipeline& pipeline) {
  wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);

  wgpuRenderPassEncoderSetVertexBuffer(renderPass,
                                       0,
                                       meshSource->vertexBuffer.Buffer,
                                       0,
                                       meshSource->vertexBuffer.Size);

  wgpuRenderPassEncoderSetIndexBuffer(renderPass,
                                      meshSource->indexBuffer.Buffer,
                                      WGPUIndexFormat_Uint32,
                                      0,
                                      meshSource->indexBuffer.Size);

  SceneUniform entries[RENDER_BATCH_SIZE];

  wgpuRenderPassEncoderSetBindGroup(renderPass, 0, bgTransform->bindGroup, 0, 0);

  int drawOffset = 0;
  for (const auto& [materialIndex, subMeshesIndexes] : materialSubmeshes) {
    wgpuRenderPassEncoderSetBindGroup(renderPass, 2, meshSource->m_Materials[materialIndex]->bgMaterial, 0, 0);

    for (int subMeshIndex : subMeshesIndexes) {
      const SubMesh& subMesh = meshSource->m_SubMeshes[subMeshIndex];
      int drawCount = subMeshNodes[subMeshIndex].size();
      wgpuRenderPassEncoderDrawIndexed(renderPass, subMesh.IndexCount, drawCount, subMesh.BaseIndex, subMesh.BaseVertex, drawOffset);

      for (int i = 0; i < drawCount; i++) {
        entries[drawOffset + i].modelMatrix = subMeshNodes[subMeshIndex][i]->LocalTransform;
      }
      drawOffset += drawCount;
    }
  }

  wgpuQueueWriteBuffer(Render::Instance->m_queue, bgTransform->buffer.Buffer, 0, &entries[0], sizeof(SceneUniform) * drawOffset);
}

void RenderQueue::Clear() {
  //_renderList.clear();
  _renderQueue.clear();
}
