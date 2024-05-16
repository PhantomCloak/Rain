#pragma once
#include <webgpu/webgpu.h>
#include <glm/glm.hpp>
#include <memory>
#include "Cam.h"

struct CameraUniform {
  glm::mat4x4 projectionMatrix;
  glm::mat4x4 viewMatrix;
};

class Camera {
 public:
  Camera() {};
  Camera(glm::mat4x4 projection, std::shared_ptr<PlayerCamera> camera, WGPUDevice device, WGPUBindGroupLayout& resourceLayout);
  Camera(glm::mat4x4 projection,glm::mat4x4 view, WGPUDevice device, WGPUBindGroupLayout& resourceLayout);
  void updateBuffer(WGPUQueue queue);
  void updateBufferStandalone(WGPUQueue queue);

  CameraUniform uniform;
  WGPURenderPipeline m_pipe;

  void createUniformBuffer(WGPUDevice device);
  void createBindGroup(WGPUDevice device, WGPUBindGroupLayout bindGroupLayout);
  WGPUBindGroup bindGroup;
  WGPUBuffer uniformBuffer;
	private:
	std::shared_ptr<PlayerCamera> cameraInstance = nullptr;
};
