#pragma once

#include <webgpu/webgpu.h>

struct GLFWwindow;

// Creates a WGPUSurface from a GLFW window (cross-platform: macOS + Windows)
WGPUSurface createSurfaceForWindow(WGPUInstance instance, GLFWwindow *window);
