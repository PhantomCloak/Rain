#include "ImGuiLayer.h"
#include "Application.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include "core/Ref.h"
#include "imgui.h"
#include "render/RenderContext.h"
#include "render/RenderUtils.h"

namespace Rain
{
  void ImGuiLayer::OnAttach()
  {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.FontGlobalScale = 1.0f;

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(1.0f);

    ImGui_ImplGlfw_InitForOther(static_cast<GLFWwindow*>(Application::Get()->GetNativeWindow()), true);

    ImGui_ImplWGPU_InitInfo initInfo;
    initInfo.Device = RenderContext::GetDevice();
    // initInfo.RenderTargetFormat = render->m_swapChainFormat;
    initInfo.RenderTargetFormat = WGPUTextureFormat_BGRA8Unorm;
    initInfo.PipelineMultisampleState = {
        .nextInChain = nullptr,
        .count = 1,
        .mask = ~0u,
        .alphaToCoverageEnabled = false};
    initInfo.DepthStencilFormat = WGPUTextureFormat_Undefined;
    ImGui_ImplWGPU_Init(&initInfo);
  }

  void ImGuiLayer::OnDeattach()
  {
  }

  void ImGuiLayer::Begin()
  {
    m_CurrentTextureView = Application::Get()->GetSwapChain()->GetSurfaceTextureView();

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::DockSpaceOverViewport();
  }

  void ImGuiLayer::End()
  {
    ImGui::EndFrame();
    ImGui::Render();

    if (m_CurrentTextureView == nullptr)
    {
      return;
    }

    WGPUDevice device = RenderContext::GetDevice();
    WGPUQueue queue = *RenderContext::GetQueue();

    WGPUCommandEncoderDescriptor encoderDesc{};
    ZERO_INIT(encoderDesc);
    encoderDesc.label = RenderUtils::MakeLabel("ImGui Command Encoder");
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

    WGPURenderPassColorAttachment colorAttachment{};
    ZERO_INIT(colorAttachment);
    colorAttachment.nextInChain = nullptr;
    colorAttachment.view = m_CurrentTextureView;
    colorAttachment.resolveTarget = nullptr;
    colorAttachment.loadOp = WGPULoadOp_Load;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    colorAttachment.clearValue = {0.0f, 0.0f, 0.0f, 1.0f};
#ifndef __EMSCRIPTEN__
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

    WGPURenderPassDescriptor renderPassDesc{};
    ZERO_INIT(renderPassDesc);
    renderPassDesc.label = RenderUtils::MakeLabel("ImGui Render Pass");
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &colorAttachment;
    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.nextInChain = nullptr;

    WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);

    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);

    WGPUCommandBufferDescriptor cmdBufferDesc{};
    ZERO_INIT(cmdBufferDesc);
    cmdBufferDesc.label = RenderUtils::MakeLabel("ImGui Command Buffer");
    WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);

    wgpuQueueSubmit(queue, 1, &cmdBuffer);

    wgpuCommandBufferRelease(cmdBuffer);
    wgpuCommandEncoderRelease(encoder);
    wgpuTextureViewRelease(m_CurrentTextureView);
    m_CurrentTextureView = nullptr;
  }

}  // namespace Rain
