#include "Mesh.h"
#include <iostream>
#include "render/Render.h"
#include "render/RenderUtils.h"

Mesh::Mesh(std::vector<VertexAttribute> vertices,
           std::vector<unsigned int> indices,
           std::shared_ptr<Texture> textureDiffuse,
           std::shared_ptr<Texture> textureHeight,
           MaterialUniform material,
           WGPUSampler& sampler) {
  this->vertices = vertices;
  this->indices = indices;
  this->textureDiffuse = textureDiffuse;
  this->materialUniform = material;

  WGPUBufferDescriptor materialUniformDesc = {};
  materialUniformDesc.size = sizeof(MaterialUniform);
  materialUniformDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
  materialUniformDesc.mappedAtCreation = false;

  materialUniformBuffer = wgpuDeviceCreateBuffer(Render::Instance->m_device, &materialUniformDesc);

  static std::vector<WGPUBindGroupEntry> bindingsMaterial(3);

  bindingsMaterial[0].binding = 0;
  bindingsMaterial[0].offset = 0;
  bindingsMaterial[0].textureView = textureDiffuse->View;

  bindingsMaterial[1].binding = 1;
  bindingsMaterial[1].offset = 0;
  bindingsMaterial[1].sampler = sampler;

  // bindingsMaterial[2].binding = 3;
  // bindingsMaterial[2].offset = 0;
  // bindingsMaterial[2].textureView = textureHeight->View;

  bindingsMaterial[2].binding = 2;
  bindingsMaterial[2].buffer = materialUniformBuffer;
  bindingsMaterial[2].offset = 0;
  bindingsMaterial[2].size = sizeof(MaterialUniform);

  GroupLayout materialGroup = {
      {0, GroupLayoutVisibility::Fragment, GroupLayoutType::Texture},
      {1, GroupLayoutVisibility::Fragment, GroupLayoutType::Sampler},
      {2, GroupLayoutVisibility::Both, GroupLayoutType::Uniform}};

  static bool first = true;

  static WGPUBindGroupLayout materialLayout;

  if (first) {
    materialLayout = LayoutUtils::CreateBindGroup("bgl_mesh_mat", Render::Instance->m_device, materialGroup);
  }

  WGPUBindGroupDescriptor bgMaterialDesc = {.label = "bg_mesh_mat"};
  bgMaterialDesc.layout = materialLayout;
  bgMaterialDesc.entryCount = (uint32_t)bindingsMaterial.size();
  bgMaterialDesc.entries = bindingsMaterial.data();

  bgMaterial = wgpuDeviceCreateBindGroup(Render::Instance->m_device, &bgMaterialDesc);

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
  wgpuQueueWriteBuffer(Render::Instance->m_queue, materialUniformBuffer, 0, &materialUniform, sizeof(MaterialUniform));
}

void Mesh::Draw(WGPURenderPassEncoder& renderPass, WGPURenderPipeline& pipeline) {

  wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer, 0, vertices.size() * sizeof(VertexAttribute));
  wgpuRenderPassEncoderSetIndexBuffer(renderPass, indexBuffer, WGPUIndexFormat_Uint32, 0, indices.size() * sizeof(unsigned int));

  wgpuRenderPassEncoderSetBindGroup(renderPass, 2, bgMaterial, 0, NULL);

  wgpuRenderPassEncoderDrawIndexed(renderPass, indices.size(), 1, 0, 0, 0);
}

void Mesh::UpdateUniforms(WGPUQueue& queue) {
  std::cout << "TODO: Update uniforms" << std::endl;
}
