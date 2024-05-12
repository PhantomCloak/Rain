#include "Camera.h"
#include <glm/ext.hpp>
#include <vector>
#include "glm/ext/matrix_transform.hpp"

Camera::Camera(WGPUDevice device, WGPURenderPipeline pipe, glm::mat4 projectionMatrix, glm::mat4 viewMatrix) {
  createUniformBuffer(device);
  float screenWidth = 1920;
  float screenHeight = 1080;
  float PI = 3.14159265358979323846f;
  //uniform.viewMatrix = glm::lookAt(glm::vec3(-2.0f, -3.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0, 0, 1));
  uniform.viewMatrix = glm::mat4(1);
  uniform.projectionMatrix = glm::perspective(45 * PI / 180, screenWidth / screenHeight, 0.01f, 4000.0f);
  m_pipe = pipe;
  WGPUBindGroupLayout layout = wgpuRenderPipelineGetBindGroupLayout(m_pipe, 1);
  createBindGroup(device, layout);
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
  wgpuQueueWriteBuffer(queue, uniformBuffer, 0, &uniform, sizeof(CameraUniform));
}

void Camera::LookAt(WGPUQueue queue, glm::vec3 pos) {
  glm::vec3 target = cameraPos + pos;
  uniform.viewMatrix = glm::lookAt(cameraPos, target, cameraUp);
  updateBuffer(queue);
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
