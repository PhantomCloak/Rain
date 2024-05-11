#include "RenderMesh.h"
#include <vector>

RenderMesh::RenderMesh(WGPUDevice device, WGPURenderPipeline pipe, WGPUTexture texture, WGPUTextureView textureView, WGPUSampler sampler) {
    createUniformBuffer(device);
    meshTexture = texture;
    meshTextureView = textureView;
    uniform = {};
    m_pipe = pipe;
    static WGPUBindGroupLayout  layout = wgpuRenderPipelineGetBindGroupLayout(m_pipe, 0);
    createBindGroup(device, layout, meshTextureView, sampler);
}

RenderMesh::~RenderMesh() {
    wgpuTextureViewRelease(meshTextureView);
    wgpuTextureDestroy(meshTexture);
    wgpuTextureRelease(meshTexture);

    wgpuBufferDestroy(vertexBuffer);
    wgpuBufferRelease(vertexBuffer);
}

void RenderMesh::updateBuffer(WGPUQueue queue) {
    wgpuQueueWriteBuffer(queue, uniformBuffer, 0, &uniform, sizeof(RenderMeshUniform));
}


void RenderMesh::createUniformBuffer(WGPUDevice device) {
    static WGPUBufferDescriptor uniformBufferDesc = {};
    uniformBufferDesc.size = sizeof(RenderMeshUniform);
    uniformBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
    uniformBufferDesc.mappedAtCreation = false;

    uniformBuffer = wgpuDeviceCreateBuffer(device, &uniformBufferDesc);
}

void RenderMesh::SetVertexBuffer(WGPUDevice device, WGPUQueue queue, std::vector<VertexAttributes> vertexData) {
    meshVertexData = vertexData;

    createVertexBuffer(device, vertexData.size());
    wgpuQueueWriteBuffer(queue, vertexBuffer, 0, vertexData.data(), vertexBufferDesc.size);

    meshVertexCount = vertexData.size();
}

void RenderMesh::SetVertexBuffer2(WGPUDevice device, WGPUQueue queue, std::vector<VertexE> vertexData) {
    //meshVertexData = vertexData;

    createVertexBuffer(device, vertexData.size());
    wgpuQueueWriteBuffer(queue, vertexBuffer, 0, vertexData.data(), vertexBufferDesc.size);

    meshVertexCount = vertexData.size();
}

void RenderMesh::createVertexBuffer(WGPUDevice device, int size) {
    vertexBufferDesc = {};
    vertexBufferDesc.size = size * sizeof(VertexAttributes);
    vertexBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
    vertexBufferDesc.mappedAtCreation = false;
    vertexBuffer = wgpuDeviceCreateBuffer(device, &vertexBufferDesc);
}

void RenderMesh::createBindGroup(WGPUDevice device, WGPUBindGroupLayout bindGroupLayout, WGPUTextureView textureView, WGPUSampler sampler) {
    static std::vector<WGPUBindGroupEntry> bindingsOne(3);

    bindingsOne[0].binding = 0;
    bindingsOne[0].buffer = uniformBuffer;
    bindingsOne[0].offset = 0;
    bindingsOne[0].size = sizeof(RenderMeshUniform);

    bindingsOne[1].binding = 1;
    bindingsOne[1].offset = 0;
    bindingsOne[1].textureView = textureView;

    bindingsOne[2].binding = 2;
    bindingsOne[2].offset = 0;
    bindingsOne[2].sampler = sampler;

    WGPUBindGroupDescriptor bindGroupOneDesc = {};
    bindGroupOneDesc.layout = bindGroupLayout;
    bindGroupOneDesc.entryCount = (uint32_t)bindingsOne.size();
    bindGroupOneDesc.entries = bindingsOne.data();

    bindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupOneDesc);
}

void RenderMesh::SetRenderPass(WGPURenderPassEncoder renderPass) {
    wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer, 0, meshVertexCount * sizeof(VertexAttributes));
    wgpuRenderPassEncoderSetBindGroup(renderPass, 0, bindGroup, 0, NULL);
}
