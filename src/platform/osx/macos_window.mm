#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>

#define GLFW_EXPOSE_NATIVE_COCOA

#include <GLFW/glfw3native.h> // Include GLFW's native access header
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

extern "C" WGPUSurface glfwGetWGPUSurface(WGPUInstance instance, GLFWwindow* window) {
    id metal_layer = NULL;
    NSWindow* ns_window = (NSWindow*)glfwGetCocoaWindow(window); // Correctly get NSWindow
    [ns_window.contentView setWantsLayer:YES];
    metal_layer = [CAMetalLayer layer];
    [ns_window.contentView setLayer:metal_layer];

    WGPUSurfaceDescriptorFromMetalLayer metalLayerDesc = {
        .chain = (WGPUChainedStruct){
            .next = NULL,
            .sType = WGPUSType_SurfaceDescriptorFromMetalLayer,
        },
        .layer = metal_layer,
    };

    WGPUSurfaceDescriptor descriptor = {
        .label = NULL,
        .nextInChain = (const WGPUChainedStruct*)&metalLayerDesc
    };

    return wgpuInstanceCreateSurface(instance, &descriptor);
}

