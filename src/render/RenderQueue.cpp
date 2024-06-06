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

// MS
//  -> int MS, vec<Materials>
//		-> int MatIdx, vec<SubMesh>
//			-> int SubMeshId, vec<Nodes>


typedef int MeshSourceIndex;

typedef std::unordered_map<int, std::vector<int>> SubMeshTable;
typedef std::unordered_map<int, std::vector<int>> MaterialTable;

std::vector<BatchGroup> _batchGroups;

// MeshSource -> SubMesh

std::unordered_map<MeshSourceIndex, Ref<MeshSource>> indexedMeshSources;
std::unordered_map<MeshSourceIndex, MaterialTable> materialSubmeshes;
std::unordered_map<MeshSourceIndex, SubMeshTable> subMeshNodes;

// Ref<MeshSource> meshSource = nullptr;

void RenderQueue::IndexMeshSource(Ref<MeshSource> meshSource) {
  if (indexedMeshSources.find(meshSource->Id) != indexedMeshSources.end()) {
    return;
  }

  indexedMeshSources[meshSource->Id] = meshSource;

  for (int i = 0; i < meshSource->m_SubMeshes.size(); i++) {
    const SubMesh& subMesh = meshSource->m_SubMeshes[i];
    materialSubmeshes[meshSource->Id][subMesh.MaterialIndex].push_back(i);
  }
}

void RenderQueue::AddQueue(RenderQueueItem item) {
  subMeshNodes[item.MeshSourceId][item.Node.SubMeshId].push_back(item.NodeIdx);
}

// Thus it shall be simple yet efficent "enough" batch loop
void RenderQueue::DrawEntities(WGPURenderPassEncoder& renderPass, WGPURenderPipeline& pipeline) {
  wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);

  // TODO: Stacksize on emscripten
  static SceneUniform entries[RENDER_BATCH_SIZE];

  wgpuRenderPassEncoderSetBindGroup(renderPass, 0, bgTransform->bindGroup, 0, 0);

  for (const auto& [sourceIndex, meshSource] : indexedMeshSources) {
    int drawOffset = 0;

    wgpuRenderPassEncoderSetVertexBuffer(renderPass,
                                         0,
                                         meshSource->m_VertexBuffer.Buffer,
                                         0,
                                         meshSource->m_VertexBuffer.Size);

    wgpuRenderPassEncoderSetIndexBuffer(renderPass,
                                        meshSource->m_IndexBuffer.Buffer,
                                        WGPUIndexFormat_Uint32,
                                        0,
                                        meshSource->m_IndexBuffer.Size);

    for (const auto& [materialIndex, subMeshesIndexes] : materialSubmeshes[sourceIndex]) {
      wgpuRenderPassEncoderSetBindGroup(renderPass, 2, meshSource->m_Materials[materialIndex]->bgMaterial, 0, 0);

      for (int subMeshIndex : subMeshesIndexes) {
        const SubMesh& subMesh = meshSource->m_SubMeshes[subMeshIndex];

				int drawCount = subMeshNodes[sourceIndex][subMeshIndex].size();

        wgpuRenderPassEncoderDrawIndexed(renderPass, subMesh.IndexCount, drawCount, subMesh.BaseIndex, subMesh.BaseVertex, drawOffset);

        for (int i = 0; i < drawCount; i++) {
          Ref<MeshNode> node = meshSource->m_Nodes[subMeshNodes[sourceIndex][subMeshIndex][i]];
          entries[drawOffset + i].modelMatrix = node->LocalTransform;
        }
        drawOffset += drawCount;
      }
    }
    wgpuQueueWriteBuffer(Render::Instance->m_queue, bgTransform->buffer.Buffer, 0, &entries[0], sizeof(SceneUniform) * drawOffset);
  }
}

void RenderQueue::Clear() {

	for(const auto& [_, subMesh] : subMeshNodes)
	{
		subMeshNodes.clear();
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
