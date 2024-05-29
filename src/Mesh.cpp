#include "Mesh.h"
#include <iostream>
#include "render/Render.h"
#include "render/RenderQueue.h"
#include "render/RenderUtils.h"

static int meshId = 0;

Mesh::Mesh(std::vector<VertexAttribute> vertices,
           std::vector<unsigned int> indices) {
	this->Id = meshId++;
  this->vertices = vertices;
  this->indices = indices;

  WGPUBufferDescriptor materialUniformDesc = {};
  materialUniformDesc.size = sizeof(MaterialUniform);
  materialUniformDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
  materialUniformDesc.mappedAtCreation = false;

  GroupLayout materialGroup = {
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {1, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler},
      {2, GroupLayoutVisibility::Both, GroupLayoutType::Uniform}};

  static bool first = true;

  static WGPUBindGroupLayout materialLayout;

  if (first) {
    materialLayout = LayoutUtils::CreateBindGroup("bgl_mesh_mat", Render::Instance->m_device, materialGroup);
  }

  WGPUBufferDescriptor vertexBufferDesc = {};
  vertexBufferDesc.size = vertices.size() * sizeof(VertexAttribute);
  vertexBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
  vertexBufferDesc.mappedAtCreation = false;
  vertexBuffer = wgpuDeviceCreateBuffer(Render::Instance->m_device, &vertexBufferDesc);

  WGPUBufferDescriptor indexBufferDesc = {};
  indexBufferDesc.size = indices.size() * sizeof(unsigned int);
  indexBufferDesc.size = (indexBufferDesc.size + 3) & ~3;
  indexBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
  indexBufferDesc.mappedAtCreation = false;

  indexBuffer = wgpuDeviceCreateBuffer(Render::Instance->m_device, &indexBufferDesc);

  wgpuQueueWriteBuffer(Render::Instance->m_queue, vertexBuffer, 0, vertices.data(), vertexBufferDesc.size);
  wgpuQueueWriteBuffer(Render::Instance->m_queue, indexBuffer, 0, indices.data(), indexBufferDesc.size);
}


