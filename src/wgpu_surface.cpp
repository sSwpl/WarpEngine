#include "wgpu_surface.h"

#include <GLFW/glfw3.h>

#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#include <GLFW/glfw3native.h>

#ifdef __APPLE__
// Implemented in wgpu_surface_macos.mm
extern "C" void *getMetalLayerFromWindow(void *nsWindow);
#endif

WGPUSurface createSurfaceForWindow(WGPUInstance instance, GLFWwindow *window) {
#ifdef __APPLE__
  // --- macOS: Metal backend ---
  void *nsWindow = (void *)glfwGetCocoaWindow(window);

  WGPUSurfaceDescriptorFromMetalLayer metalDesc = {};
  metalDesc.chain.next = nullptr;
  metalDesc.chain.sType = WGPUSType_SurfaceDescriptorFromMetalLayer;
  metalDesc.layer = getMetalLayerFromWindow(nsWindow);

  WGPUSurfaceDescriptor surfDesc = {};
  surfDesc.nextInChain = &metalDesc.chain;
  surfDesc.label = "WarpEngine Surface (macOS)";

  return wgpuInstanceCreateSurface(instance, &surfDesc);

#elif defined(_WIN32)
  // --- Windows: D3D12/Vulkan backend ---
  void *hwnd = (void *)glfwGetWin32Window(window);

  WGPUSurfaceDescriptorFromWindowsHWND winDesc = {};
  winDesc.chain.next = nullptr;
  winDesc.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
  winDesc.hinstance = GetModuleHandle(NULL);
  winDesc.hwnd = hwnd;

  WGPUSurfaceDescriptor surfDesc = {};
  surfDesc.nextInChain = &winDesc.chain;
  surfDesc.label = "WarpEngine Surface (Windows)";

  return wgpuInstanceCreateSurface(instance, &surfDesc);

#else
#error "Unsupported platform for surface creation"
  return nullptr;
#endif
}
