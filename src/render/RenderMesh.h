#pragma once
#include <webgpu/webgpu.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "primitives/mesh.h"
#include "primitives/RenderMeshUniform.h"
#include "Mesh.h"

class RenderMesh {
 public:
  RenderMeshUniform uniform;
  int meshVertexCount;
  std::string name;

  RenderMesh(WGPUDevice device, WGPURenderPipeline pipe, WGPUTexture texture, WGPUTextureView textureView, WGPUSampler sampler);
  ~RenderMesh();
  void updateBuffer(WGPUQueue queue);
  void SetRenderPass(WGPURenderPassEncoder renderPass);
  void SetVertexBuffer(WGPUDevice device, WGPUQueue queue, std::vector<VertexAttributes> vertexData);
  void SetVertexBuffer2(WGPUDevice device, WGPUQueue queue, std::vector<VertexE> vertexData);

 private:
  WGPURenderPipeline m_pipe;
  WGPUTexture meshTexture;
  WGPUTextureView meshTextureView;
  WGPUBuffer uniformBuffer;
  WGPUBindGroup bindGroup;
  WGPUBuffer vertexBuffer;
  WGPUBufferDescriptor vertexBufferDesc;
  std::vector<VertexAttributes> meshVertexData;

  void createUniformBuffer(WGPUDevice device);
  void createVertexBuffer(WGPUDevice device, int size);
  void createBindGroup(WGPUDevice device, WGPUBindGroupLayout bindGroupLayout, WGPUTextureView textureView, WGPUSampler sampler);
};
