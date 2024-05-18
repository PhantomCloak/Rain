#include "Mesh.h"
#include <iostream>

Mesh::Mesh(std::vector<VertexAttribute> vertices,
             std::vector<unsigned int> indices,
             std::shared_ptr<Texture> textureDiffuse,
						 std::shared_ptr<Texture> textureHeight,
						 MaterialUniform material,
             WGPUBindGroupLayout resourceLayout,
             WGPUDevice& device,
             WGPUQueue& queue,
             WGPUSampler& sampler)
    : Node() {
  this->vertices = vertices;
  this->indices = indices;
  this->textureDiffuse = textureDiffuse;
	this->materialUniform = material;

  sceneUniform = {};
  if (Parent != nullptr) {
    sceneUniform.modelMatrix = Parent->GetModelMatrix() * GetModelMatrix();
  } else {
    sceneUniform.modelMatrix = GetModelMatrix();
  }

  WGPUBufferDescriptor sceneUniformDesc = {};
  sceneUniformDesc.size = sizeof(SceneUniform);
  sceneUniformDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
  sceneUniformDesc.mappedAtCreation = false;

  sceneUniformBuffer = wgpuDeviceCreateBuffer(device, &sceneUniformDesc);

  WGPUBufferDescriptor materialUniformDesc = {};
  materialUniformDesc.size = sizeof(MaterialUniform);
  materialUniformDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
  materialUniformDesc.mappedAtCreation = false;

  materialUniformBuffer = wgpuDeviceCreateBuffer(device, &materialUniformDesc);

  static std::vector<WGPUBindGroupEntry> bindingsOne(5);

  bindingsOne[0].binding = 0;
  bindingsOne[0].buffer = sceneUniformBuffer;
  bindingsOne[0].offset = 0;
  bindingsOne[0].size = sizeof(SceneUniform);

  bindingsOne[1].binding = 1;
  bindingsOne[1].offset = 0;
  bindingsOne[1].textureView = textureDiffuse->View;

  bindingsOne[2].binding = 2;
  bindingsOne[2].offset = 0;
  bindingsOne[2].sampler = sampler;

  bindingsOne[3].binding = 3;
  bindingsOne[3].offset = 0;
  bindingsOne[3].textureView = textureHeight->View;

  bindingsOne[4].binding = 4;
  bindingsOne[4].buffer = materialUniformBuffer;
  bindingsOne[4].offset = 0;
  bindingsOne[4].size = sizeof(MaterialUniform);

	WGPUBindGroupDescriptor bindGroupOneDesc = { .label = "bg_mesh"};
  bindGroupOneDesc.layout = resourceLayout;
  bindGroupOneDesc.entryCount = (uint32_t)bindingsOne.size();
  bindGroupOneDesc.entries = bindingsOne.data();

  defaultResourcesBindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupOneDesc);

  WGPUBufferDescriptor vertexBufferDesc = {};
  vertexBufferDesc.size = vertices.size() * sizeof(VertexAttribute);
  vertexBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
  vertexBufferDesc.mappedAtCreation = false;
  vertexBuffer = wgpuDeviceCreateBuffer(device, &vertexBufferDesc);

  WGPUBufferDescriptor indexBufferDesc = {};
  indexBufferDesc.size = indices.size() * sizeof(unsigned int);
  indexBufferDesc.size = (indexBufferDesc.size + 3) & ~3;
  indexBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
  indexBufferDesc.mappedAtCreation = false;

  indexBuffer = wgpuDeviceCreateBuffer(device, &indexBufferDesc);

  wgpuQueueWriteBuffer(queue, vertexBuffer, 0, vertices.data(), vertexBufferDesc.size);
  wgpuQueueWriteBuffer(queue, indexBuffer, 0, indices.data(), indexBufferDesc.size);
  wgpuQueueWriteBuffer(queue, sceneUniformBuffer, 0, &sceneUniform, sizeof(SceneUniform));
  wgpuQueueWriteBuffer(queue, materialUniformBuffer, 0, &materialUniform, sizeof(MaterialUniform));
}

void Mesh::Draw(WGPURenderPassEncoder& renderPass, WGPURenderPipeline& pipeline) {
  wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);

  wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer, 0, vertices.size() * sizeof(VertexAttribute));
  wgpuRenderPassEncoderSetIndexBuffer(renderPass, indexBuffer, WGPUIndexFormat_Uint32, 0, indices.size() * sizeof(unsigned int));

  wgpuRenderPassEncoderSetBindGroup(renderPass, 0, defaultResourcesBindGroup, 0, NULL);

  wgpuRenderPassEncoderDrawIndexed(renderPass, indices.size(), 1, 0, 0, 0);
}

void Mesh::UpdateUniforms(WGPUQueue& queue) {
  if (Parent != nullptr) {
    sceneUniform.modelMatrix = Parent->GetModelMatrix() * GetModelMatrix();
  } else {
    sceneUniform.modelMatrix = GetModelMatrix();
  }
  wgpuQueueWriteBuffer(queue, sceneUniformBuffer, 0, &sceneUniform, sizeof(SceneUniform));
}
