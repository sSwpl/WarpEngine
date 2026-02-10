#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

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

// Pomocnicza funkcja: nazwa typu adaptera
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

// Pomocnicza funkcja: nazwa backendu graficznego
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

int main() {
  // 1. Initialize GLFW
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW!" << std::endl;
    return -1;
  }

  // 2. Create WebGPU instance
  WGPUInstance instance = wgpuCreateInstance(nullptr);
  if (!instance) {
    std::cerr << "Failed to create WebGPU instance!" << std::endl;
    glfwTerminate();
    return -1;
  }

  // 3. Request GPU adapter (async callback, ale wgpu-native wykonuje
  // synchronicznie)
  WGPUAdapter adapter = nullptr;
  WGPURequestAdapterOptions adapterOpts = {};
  adapterOpts.nextInChain = nullptr;
  adapterOpts.powerPreference = WGPUPowerPreference_HighPerformance;
  wgpuInstanceRequestAdapter(instance, &adapterOpts, onAdapterRequestEnded,
                             &adapter);

  if (!adapter) {
    std::cerr << "Failed to get a WebGPU adapter!" << std::endl;
    wgpuInstanceRelease(instance);
    glfwTerminate();
    return -1;
  }

  // 4. Get adapter properties (GPU info)
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

  // 5. Set window title with GPU name
  std::string windowTitle =
      "WarpEngine | " + std::string(props.name ? props.name : "Unknown GPU");

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow *window =
      glfwCreateWindow(800, 600, windowTitle.c_str(), nullptr, nullptr);

  if (!window) {
    std::cerr << "Failed to create GLFW window!" << std::endl;
    wgpuAdapterRelease(adapter);
    wgpuInstanceRelease(instance);
    glfwTerminate();
    return -1;
  }

  std::cout << "\nWarpEngine started! Window open." << std::endl;

  // 6. Main loop
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    // Rendering magic will happen here
  }

  // 7. Cleanup
  glfwDestroyWindow(window);
  wgpuAdapterRelease(adapter);
  wgpuInstanceRelease(instance);
  glfwTerminate();
  return 0;
}
