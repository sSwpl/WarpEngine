#include <GLFW/glfw3.h>
#include <iostream>
#include <webgpu/webgpu.h>

int main() {
  // 1. Initialize GLFW
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW!" << std::endl;
    return -1;
  }

  // Disable default API (since we want WebGPU, not OpenGL)
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow *window =
      glfwCreateWindow(800, 600, "WarpEngine | WebGPU Test", nullptr, nullptr);

  if (!window) {
    std::cerr << "Failed to create GLFW window!" << std::endl;
    glfwTerminate();
    return -1;
  }

  std::cout << "WarpEngine started!" << std::endl;

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    // Rendering magic will happen here
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
