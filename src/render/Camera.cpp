#include "Camera.h"
#include <glm/ext.hpp>
#include <vector>

Camera::Camera(glm::mat4x4 projection, std::shared_ptr<PlayerCamera> camera, WGPUDevice device, WGPUBindGroupLayout& resourceLayout) {
  createUniformBuffer(device);

  uniform.projectionMatrix = projection;
  uniform.viewMatrix = camera->GetViewMatrix();

  createBindGroup(device, resourceLayout);

  cameraInstance = camera;
}

void Camera::createUniformBuffer(WGPUDevice device) {
  WGPUBufferDescriptor uniformBufferDesc = {};
  uniformBufferDesc.size = sizeof(CameraUniform);
  uniformBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
  uniformBufferDesc.mappedAtCreation = false;

  uniformBuffer = wgpuDeviceCreateBuffer(device, &uniformBufferDesc);
}

void Camera::Begin(WGPURenderPassEncoder renderPass) {
  wgpuRenderPassEncoderSetBindGroup(renderPass, 1, bindGroup, 0, NULL);
}

void Camera::updateBuffer(WGPUQueue queue) {
  uniform.viewMatrix = cameraInstance->GetViewMatrix();
  wgpuQueueWriteBuffer(queue, uniformBuffer, 0, &uniform, sizeof(CameraUniform));
}

void Camera::createBindGroup(WGPUDevice device, WGPUBindGroupLayout bindGroupLayout) {
  std::vector<WGPUBindGroupEntry> bindingsTwo(1);

  bindingsTwo[0].binding = 0;
  bindingsTwo[0].buffer = uniformBuffer;
  bindingsTwo[0].offset = 0;
  bindingsTwo[0].size = sizeof(CameraUniform);

  WGPUBindGroupDescriptor bindGroupTwoDesc = {};
  bindGroupTwoDesc.layout = bindGroupLayout;
  bindGroupTwoDesc.entryCount = (uint32_t)bindingsTwo.size();
  bindGroupTwoDesc.entries = bindingsTwo.data();

  bindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupTwoDesc);
}
