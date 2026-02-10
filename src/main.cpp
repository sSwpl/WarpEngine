#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

#include "wgpu_surface.h"

// ============================================================
//  Callbacks
// ============================================================

// Callback wywoływany po znalezieniu adaptera GPU
void onAdapterRequestEnded(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                           char const *message, void *userdata) {
  if (status != WGPURequestAdapterStatus_Success) {
    std::cerr << "Could not get WebGPU adapter: " << (message ? message : "")
              << std::endl;
    *static_cast<WGPUAdapter *>(userdata) = nullptr;
    return;
  }
  *static_cast<WGPUAdapter *>(userdata) = adapter;
}

// Callback wywoływany po utworzeniu urządzenia GPU
void onDeviceRequestEnded(WGPURequestDeviceStatus status, WGPUDevice device,
                          char const *message, void *userdata) {
  if (status != WGPURequestDeviceStatus_Success) {
    std::cerr << "Could not get WebGPU device: " << (message ? message : "")
              << std::endl;
    *static_cast<WGPUDevice *>(userdata) = nullptr;
    return;
  }
  *static_cast<WGPUDevice *>(userdata) = device;
}

// Callback dla nieobsłużonych błędów urządzenia
void onUncapturedError(WGPUErrorType type, char const *message,
                       void * /*userdata*/) {
  std::cerr << "[WebGPU Error] type=" << type << ": "
            << (message ? message : "(no message)") << std::endl;
}

// Callback dla utraty urządzenia
void onDeviceLost(WGPUDeviceLostReason reason, char const *message,
                  void * /*userdata*/) {
  std::cerr << "[WebGPU] Device lost! reason=" << reason << ": "
            << (message ? message : "(no message)") << std::endl;
}

// ============================================================
//  Pomocnicze funkcje
// ============================================================

// Nazwa typu adaptera
const char *adapterTypeName(WGPUAdapterType type) {
  switch (type) {
  case WGPUAdapterType_DiscreteGPU:
    return "Discrete GPU";
  case WGPUAdapterType_IntegratedGPU:
    return "Integrated GPU";
  case WGPUAdapterType_CPU:
    return "CPU (software)";
  default:
    return "Unknown";
  }
}

// Nazwa backendu graficznego
const char *backendTypeName(WGPUBackendType type) {
  switch (type) {
  case WGPUBackendType_D3D12:
    return "Direct3D 12";
  case WGPUBackendType_D3D11:
    return "Direct3D 11";
  case WGPUBackendType_Vulkan:
    return "Vulkan";
  case WGPUBackendType_Metal:
    return "Metal";
  case WGPUBackendType_OpenGL:
    return "OpenGL";
  case WGPUBackendType_OpenGLES:
    return "OpenGL ES";
  default:
    return "Unknown";
  }
}

// ============================================================
//  Main
// ============================================================

int main() {
  // ── 1. Inicjalizacja GLFW ────────────────────────────────
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW!" << std::endl;
    return -1;
  }

  // ── 2. Tworzenie instancji WebGPU ────────────────────────
  WGPUInstance instance = wgpuCreateInstance(nullptr);
  if (!instance) {
    std::cerr << "Failed to create WebGPU instance!" << std::endl;
    glfwTerminate();
    return -1;
  }

  // ── 3. Tworzenie okna GLFW (bez kontekstu OpenGL) ────────
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow *window = glfwCreateWindow(
      800, 600, "WarpEngine | Initializing...", nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window!" << std::endl;
    wgpuInstanceRelease(instance);
    glfwTerminate();
    return -1;
  }

  // ── 4. Tworzenie Surface (platforma macOS / Windows) ─────
  WGPUSurface surface = createSurfaceForWindow(instance, window);
  if (!surface) {
    std::cerr << "Failed to create WebGPU surface!" << std::endl;
    glfwDestroyWindow(window);
    wgpuInstanceRelease(instance);
    glfwTerminate();
    return -1;
  }

  // ── 5. Żądanie adaptera GPU ──────────────────────────────
  WGPUAdapter adapter = nullptr;
  WGPURequestAdapterOptions adapterOpts = {};
  adapterOpts.nextInChain = nullptr;
  adapterOpts.compatibleSurface = surface;
  adapterOpts.powerPreference = WGPUPowerPreference_HighPerformance;
  wgpuInstanceRequestAdapter(instance, &adapterOpts, onAdapterRequestEnded,
                             &adapter);
  if (!adapter) {
    std::cerr << "Failed to get a WebGPU adapter!" << std::endl;
    wgpuSurfaceRelease(surface);
    wgpuInstanceRelease(instance);
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }

  // ── 6. Informacje o GPU ──────────────────────────────────
  WGPUAdapterProperties props = {};
  props.nextInChain = nullptr;
  wgpuAdapterGetProperties(adapter, &props);

  std::cout << "==========================================" << std::endl;
  std::cout << "  WarpEngine - GPU Info" << std::endl;
  std::cout << "==========================================" << std::endl;
  std::cout << "  GPU:      " << (props.name ? props.name : "N/A") << std::endl;
  std::cout << "  Vendor:   " << (props.vendorName ? props.vendorName : "N/A")
            << std::endl;
  std::cout << "  Driver:   "
            << (props.driverDescription ? props.driverDescription : "N/A")
            << std::endl;
  std::cout << "  Type:     " << adapterTypeName(props.adapterType)
            << std::endl;
  std::cout << "  Backend:  " << backendTypeName(props.backendType)
            << std::endl;
  std::cout << "==========================================" << std::endl;

  // Ustaw tytuł okna z nazwą GPU
  std::string windowTitle =
      "WarpEngine | " + std::string(props.name ? props.name : "Unknown GPU");
  glfwSetWindowTitle(window, windowTitle.c_str());

  // ── 7. Żądanie urządzenia (Device) ──────────────────────
  WGPUDevice device = nullptr;
  WGPUDeviceDescriptor deviceDesc = {};
  deviceDesc.nextInChain = nullptr;
  deviceDesc.label = "WarpEngine Device";
  deviceDesc.requiredFeatureCount = 0;
  deviceDesc.requiredFeatures = nullptr;
  deviceDesc.requiredLimits = nullptr;
  deviceDesc.defaultQueue.nextInChain = nullptr;
  deviceDesc.defaultQueue.label = "Default Queue";
  deviceDesc.deviceLostCallback = onDeviceLost;
  deviceDesc.deviceLostUserdata = nullptr;

  wgpuAdapterRequestDevice(adapter, &deviceDesc, onDeviceRequestEnded, &device);
  if (!device) {
    std::cerr << "Failed to get a WebGPU device!" << std::endl;
    wgpuSurfaceRelease(surface);
    wgpuAdapterRelease(adapter);
    wgpuInstanceRelease(instance);
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }

  // Ustaw callback dla błędów
  wgpuDeviceSetUncapturedErrorCallback(device, onUncapturedError, nullptr);

  // Pobierz kolejkę (queue)
  WGPUQueue queue = wgpuDeviceGetQueue(device);
  if (!queue) {
    std::cerr << "Failed to get WebGPU queue!" << std::endl;
    wgpuDeviceRelease(device);
    wgpuSurfaceRelease(surface);
    wgpuAdapterRelease(adapter);
    wgpuInstanceRelease(instance);
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }

  std::cout << "\nDevice and Queue acquired successfully!" << std::endl;

  // ── 8. Konfiguracja Surface ──────────────────────────────
  WGPUSurfaceConfiguration surfConfig = {};
  surfConfig.nextInChain = nullptr;
  surfConfig.device = device;
  surfConfig.format = WGPUTextureFormat_BGRA8Unorm;
  surfConfig.usage = WGPUTextureUsage_RenderAttachment;
  surfConfig.viewFormatCount = 0;
  surfConfig.viewFormats = nullptr;
  surfConfig.alphaMode = WGPUCompositeAlphaMode_Auto;
  surfConfig.width = 800;
  surfConfig.height = 600;
  surfConfig.presentMode = WGPUPresentMode_Fifo;

  wgpuSurfaceConfigure(surface, &surfConfig);
  std::cout << "Surface configured (800x600, BGRA8Unorm, Fifo)." << std::endl;

  // ── 9. Pętla renderowania (Clear Screen) ─────────────────
  std::cout << "\nWarpEngine started! Rendering..." << std::endl;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // 9a. Pobierz bieżącą teksturę z surface
    WGPUSurfaceTexture surfaceTexture = {};
    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);

    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
      std::cerr << "Failed to get current surface texture!" << std::endl;
      break;
    }

    // 9b. Stwórz widok tekstury
    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.nextInChain = nullptr;
    viewDesc.label = "Surface Texture View";
    viewDesc.format = WGPUTextureFormat_BGRA8Unorm;
    viewDesc.dimension = WGPUTextureViewDimension_2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = 1;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = 1;
    viewDesc.aspect = WGPUTextureAspect_All;
    WGPUTextureView textureView =
        wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);

    // 9c. Stwórz command encoder
    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.nextInChain = nullptr;
    encoderDesc.label = "Command Encoder";
    WGPUCommandEncoder encoder =
        wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

    // 9d. Rozpocznij render pass — czyszczenie kolorem granatowym
    WGPURenderPassColorAttachment colorAttachment = {};
    colorAttachment.nextInChain = nullptr;
    colorAttachment.view = textureView;
    colorAttachment.resolveTarget = nullptr;
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    colorAttachment.clearValue = WGPUColor{0.05, 0.05, 0.2, 1.0};

    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.nextInChain = nullptr;
    renderPassDesc.label = "Clear Screen Pass";
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &colorAttachment;
    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.occlusionQuerySet = nullptr;
    renderPassDesc.timestampWrites = nullptr;

    WGPURenderPassEncoder renderPass =
        wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);

    // 9e. Zakończ komendę i wyślij do kolejki
    WGPUCommandBufferDescriptor cmdBufDesc = {};
    cmdBufDesc.nextInChain = nullptr;
    cmdBufDesc.label = "Render Command Buffer";
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(encoder, &cmdBufDesc);

    wgpuQueueSubmit(queue, 1, &cmdBuf);

    // 9f. Prezentuj na ekranie
    wgpuSurfacePresent(surface);

    // 9g. Zwolnij zasoby tego frame'a
    wgpuCommandBufferRelease(cmdBuf);
    wgpuCommandEncoderRelease(encoder);
    wgpuTextureViewRelease(textureView);
    wgpuTextureRelease(surfaceTexture.texture);
  }

  // ── 10. Sprzątanie zasobów ───────────────────────────────
  std::cout << "\nShutting down WarpEngine..." << std::endl;

  wgpuSurfaceUnconfigure(surface);
  wgpuQueueRelease(queue);
  wgpuDeviceRelease(device);
  wgpuSurfaceRelease(surface);
  wgpuAdapterRelease(adapter);
  wgpuInstanceRelease(instance);
  glfwDestroyWindow(window);
  glfwTerminate();

  std::cout << "Goodbye!" << std::endl;
  return 0;
}
