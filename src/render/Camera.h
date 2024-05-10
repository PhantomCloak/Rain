#pragma once
#include <webgpu/webgpu.h>
#include <glm/glm.hpp>

struct CameraUniform {
  glm::mat4x4 projectionMatrix;
  glm::mat4x4 viewMatrix;
};

constexpr float PI = 3.14159265358979323846f;

class Camera {
 public:
  Camera(WGPUDevice device, WGPURenderPipeline pipe, glm::mat4 projectionMatrix, glm::mat4 viewMatrix);
  void updateBuffer(WGPUQueue queue);
  void Begin(WGPURenderPassEncoder renderPass);
  void LookAt(WGPUQueue queue, glm::vec3 pos);

  glm::vec3 cameraPos = glm::vec3(0.0f, 5.0f, 3.0f);
  glm::vec3 cameraFront = glm::vec3(0.0f, -0.5f, 0);
  glm::vec3 cameraUp = glm::vec3(0.0f, 0.0f, 1.0f);
  glm::vec3 cameraDown = glm::vec3(0.0f, 0.0f, -1.0f);

  CameraUniform uniform;
 private:
  WGPURenderPipeline m_pipe;
  void createUniformBuffer(WGPUDevice device);
  void createBindGroup(WGPUDevice device, WGPUBindGroupLayout bindGroupLayout);
  WGPUBindGroup bindGroup;
  WGPUBuffer uniformBuffer;
};
