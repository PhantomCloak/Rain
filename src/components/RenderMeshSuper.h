#pragma once
#include "render/primitives/mesh.h"
#include "render/primitives/mesh.h"
#include "render/primitives/RenderMeshUniform.h"
#include <webgpu/webgpu.h>


struct RenderMeshSuper {
  RenderMeshUniform uniform;
  int meshVertexCount;

  RenderMeshSuper(WGPUDevice device, WGPURenderPipeline pipe, WGPUTexture texture, WGPUTextureView textureView, WGPUSampler sampler);
  ~RenderMeshSuper();
  void updateBuffer(WGPUQueue queue);
  void SetRenderPass(WGPURenderPassEncoder renderPass);
  void SetVertexBuffer(WGPUDevice device, WGPUQueue queue, std::vector<VertexAttributes> vertexData);

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
