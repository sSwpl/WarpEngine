#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "texture.h"
#include "wgpu_surface.h"

// ============================================================
//  Struktury danych
// ============================================================

// Wierzchołek quada (prostokąta)
struct Vertex {
  float x, y; // pozycja 2D
  float u, v; // współrzędne UV tekstury
};

// Uniform buffer przesyłany do GPU — macierz projekcji + pozycja gracza
struct PlayerUniforms {
  glm::mat4 projection; // 64 bytes (4×4 floats)
  float position[2];    // x, y — 8 bytes
  float _pad[2];        // wyrównanie do 16 bajtów (std140)
}; // total: 80 bytes

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
//  WGSL Shader — Textured Quad
// ============================================================

const char *shaderSource = R"(
struct PlayerUniforms {
    projection: mat4x4<f32>,
    position: vec2f,
};

@group(0) @binding(0) var<uniform> player: PlayerUniforms;
@group(1) @binding(0) var spriteTex: texture_2d<f32>;
@group(1) @binding(1) var spriteSampler: sampler;

struct VertexInput {
    @location(0) position: vec2f,
    @location(1) uv: vec2f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = player.projection * vec4f(in.position + player.position, 0.0, 1.0);
    out.uv = in.uv;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    return textureSample(spriteTex, spriteSampler, in.uv);
}
)";

// ============================================================
//  Pipeline Creation
// ============================================================

WGPURenderPipeline createPipeline(WGPUDevice device,
                                  WGPUTextureFormat surfaceFormat,
                                  WGPUBindGroupLayout uniformBindGroupLayout,
                                  WGPUBindGroupLayout textureBindGroupLayout) {
  // 1. Shader module z kodu WGSL
  WGPUShaderModuleWGSLDescriptor wgslDesc = {};
  wgslDesc.chain.next = nullptr;
  wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
  wgslDesc.code = shaderSource;

  WGPUShaderModuleDescriptor shaderDesc = {};
  shaderDesc.nextInChain = &wgslDesc.chain;
  shaderDesc.label = "Textured Quad Shader";
  shaderDesc.hintCount = 0;
  shaderDesc.hints = nullptr;

  WGPUShaderModule shaderModule =
      wgpuDeviceCreateShaderModule(device, &shaderDesc);
  if (!shaderModule) {
    std::cerr << "Failed to create shader module!" << std::endl;
    return nullptr;
  }

  // 2. Pipeline layout z dwoma bind group layouts (group 0: uniforms, group 1:
  // texture+sampler)
  WGPUBindGroupLayout layouts[2] = {uniformBindGroupLayout,
                                    textureBindGroupLayout};

  WGPUPipelineLayoutDescriptor layoutDesc = {};
  layoutDesc.nextInChain = nullptr;
  layoutDesc.label = "Pipeline Layout";
  layoutDesc.bindGroupLayoutCount = 2;
  layoutDesc.bindGroupLayouts = layouts;

  WGPUPipelineLayout pipelineLayout =
      wgpuDeviceCreatePipelineLayout(device, &layoutDesc);

  // 3. Vertex buffer layout — opisuje format Vertex{x,y,u,v}
  WGPUVertexAttribute vertexAttribs[2] = {};
  // @location(0) position: vec2f
  vertexAttribs[0].format = WGPUVertexFormat_Float32x2;
  vertexAttribs[0].offset = 0;
  vertexAttribs[0].shaderLocation = 0;
  // @location(1) uv: vec2f
  vertexAttribs[1].format = WGPUVertexFormat_Float32x2;
  vertexAttribs[1].offset = 2 * sizeof(float);
  vertexAttribs[1].shaderLocation = 1;

  WGPUVertexBufferLayout vertexBufferLayout = {};
  vertexBufferLayout.arrayStride = sizeof(Vertex); // 4 floats = 16 bytes
  vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;
  vertexBufferLayout.attributeCount = 2;
  vertexBufferLayout.attributes = vertexAttribs;

  // 4. Color target state (format musi pasować do surface)
  WGPUBlendState blendState = {};
  blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
  blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
  blendState.color.operation = WGPUBlendOperation_Add;
  blendState.alpha.srcFactor = WGPUBlendFactor_One;
  blendState.alpha.dstFactor = WGPUBlendFactor_Zero;
  blendState.alpha.operation = WGPUBlendOperation_Add;

  WGPUColorTargetState colorTarget = {};
  colorTarget.nextInChain = nullptr;
  colorTarget.format = surfaceFormat;
  colorTarget.blend = &blendState;
  colorTarget.writeMask = WGPUColorWriteMask_All;

  // 5. Fragment state
  WGPUFragmentState fragmentState = {};
  fragmentState.nextInChain = nullptr;
  fragmentState.module = shaderModule;
  fragmentState.entryPoint = "fs_main";
  fragmentState.constantCount = 0;
  fragmentState.constants = nullptr;
  fragmentState.targetCount = 1;
  fragmentState.targets = &colorTarget;

  // 6. Pipeline descriptor
  WGPURenderPipelineDescriptor pipelineDesc = {};
  pipelineDesc.nextInChain = nullptr;
  pipelineDesc.label = "Textured Quad Pipeline";
  pipelineDesc.layout = pipelineLayout;

  // Vertex state (z vertex buffer)
  pipelineDesc.vertex.nextInChain = nullptr;
  pipelineDesc.vertex.module = shaderModule;
  pipelineDesc.vertex.entryPoint = "vs_main";
  pipelineDesc.vertex.constantCount = 0;
  pipelineDesc.vertex.constants = nullptr;
  pipelineDesc.vertex.bufferCount = 1;
  pipelineDesc.vertex.buffers = &vertexBufferLayout;

  // Primitive state
  pipelineDesc.primitive.nextInChain = nullptr;
  pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
  pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
  pipelineDesc.primitive.cullMode = WGPUCullMode_None;

  // Multisample state
  pipelineDesc.multisample.nextInChain = nullptr;
  pipelineDesc.multisample.count = 1;
  pipelineDesc.multisample.mask = ~0u;
  pipelineDesc.multisample.alphaToCoverageEnabled = false;

  // Fragment
  pipelineDesc.fragment = &fragmentState;

  // Brak depth/stencil
  pipelineDesc.depthStencil = nullptr;

  WGPURenderPipeline pipeline =
      wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

  // Shader module i pipeline layout już nie są potrzebne po utworzeniu
  // pipeline'u
  wgpuShaderModuleRelease(shaderModule);
  wgpuPipelineLayoutRelease(pipelineLayout);

  if (!pipeline) {
    std::cerr << "Failed to create render pipeline!" << std::endl;
  } else {
    std::cout << "Render pipeline created successfully." << std::endl;
  }

  return pipeline;
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

  // ── 9. Ładowanie tekstury gracza ─────────────────────────
  std::cout << "\nLoading player texture..." << std::endl;
  Texture playerTexture = loadTexture(device, queue, "assets/player.png");
  if (!playerTexture.texture) {
    std::cerr
        << "Failed to load player texture! Make sure assets/player.png exists."
        << std::endl;
    wgpuSurfaceUnconfigure(surface);
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuSurfaceRelease(surface);
    wgpuAdapterRelease(adapter);
    wgpuInstanceRelease(instance);
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }

  // ── 10. Tworzenie Vertex Buffer (Quad) ───────────────────
  // Quad składa się z 4 wierzchołków (pozycje względne + UV)
  Vertex quadVertices[4] = {
      // x,    y,     u,   v
      {0.0f, 0.0f, 0.0f, 0.0f},   // lewy górny
      {64.0f, 0.0f, 1.0f, 0.0f},  // prawy górny
      {64.0f, 64.0f, 1.0f, 1.0f}, // prawy dolny
      {0.0f, 64.0f, 0.0f, 1.0f},  // lewy dolny
  };

  WGPUBufferDescriptor vertexBufDesc = {};
  vertexBufDesc.nextInChain = nullptr;
  vertexBufDesc.label = "Quad Vertex Buffer";
  vertexBufDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
  vertexBufDesc.size = sizeof(quadVertices);
  vertexBufDesc.mappedAtCreation = false;

  WGPUBuffer vertexBuffer = wgpuDeviceCreateBuffer(device, &vertexBufDesc);
  wgpuQueueWriteBuffer(queue, vertexBuffer, 0, quadVertices,
                       sizeof(quadVertices));

  // ── 11. Tworzenie Index Buffer ───────────────────────────
  // 2 trójkąty = 6 indeksów
  uint16_t quadIndices[6] = {
      0, 1, 2, // pierwszy trójkąt
      0, 2, 3  // drugi trójkąt
  };

  WGPUBufferDescriptor indexBufDesc = {};
  indexBufDesc.nextInChain = nullptr;
  indexBufDesc.label = "Quad Index Buffer";
  indexBufDesc.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
  indexBufDesc.size = sizeof(quadIndices);
  indexBufDesc.mappedAtCreation = false;

  WGPUBuffer indexBuffer = wgpuDeviceCreateBuffer(device, &indexBufDesc);
  wgpuQueueWriteBuffer(queue, indexBuffer, 0, quadIndices, sizeof(quadIndices));

  std::cout << "Vertex and index buffers created." << std::endl;

  // ── 12. Tworzenie Uniform Buffer i Bind Group (group 0) ──
  PlayerUniforms playerUniforms = {};
  playerUniforms.projection =
      glm::ortho(0.0f, 800.0f, 600.0f, 0.0f, -1.0f, 1.0f);
  playerUniforms.position[0] = 100.0f;
  playerUniforms.position[1] = 100.0f;
  playerUniforms._pad[0] = 0.0f;
  playerUniforms._pad[1] = 0.0f;

  // Uniform buffer (Uniform | CopyDst)
  WGPUBufferDescriptor uniformBufDesc = {};
  uniformBufDesc.nextInChain = nullptr;
  uniformBufDesc.label = "Player Uniform Buffer";
  uniformBufDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
  uniformBufDesc.size = sizeof(PlayerUniforms);
  uniformBufDesc.mappedAtCreation = false;
  WGPUBuffer uniformBuffer = wgpuDeviceCreateBuffer(device, &uniformBufDesc);

  // Bind group layout (group 0: uniform buffer)
  WGPUBindGroupLayoutEntry uniformBglEntry = {};
  uniformBglEntry.nextInChain = nullptr;
  uniformBglEntry.binding = 0;
  uniformBglEntry.visibility = WGPUShaderStage_Vertex;
  uniformBglEntry.buffer.nextInChain = nullptr;
  uniformBglEntry.buffer.type = WGPUBufferBindingType_Uniform;
  uniformBglEntry.buffer.hasDynamicOffset = false;
  uniformBglEntry.buffer.minBindingSize = sizeof(PlayerUniforms);
  uniformBglEntry.sampler.type = WGPUSamplerBindingType_Undefined;
  uniformBglEntry.texture.sampleType = WGPUTextureSampleType_Undefined;
  uniformBglEntry.storageTexture.access = WGPUStorageTextureAccess_Undefined;

  WGPUBindGroupLayoutDescriptor uniformBglDesc = {};
  uniformBglDesc.nextInChain = nullptr;
  uniformBglDesc.label = "Uniform Bind Group Layout";
  uniformBglDesc.entryCount = 1;
  uniformBglDesc.entries = &uniformBglEntry;
  WGPUBindGroupLayout uniformBindGroupLayout =
      wgpuDeviceCreateBindGroupLayout(device, &uniformBglDesc);

  // Bind group — łączy bufor z layoutem
  WGPUBindGroupEntry uniformBgEntry = {};
  uniformBgEntry.nextInChain = nullptr;
  uniformBgEntry.binding = 0;
  uniformBgEntry.buffer = uniformBuffer;
  uniformBgEntry.offset = 0;
  uniformBgEntry.size = sizeof(PlayerUniforms);
  uniformBgEntry.sampler = nullptr;
  uniformBgEntry.textureView = nullptr;

  WGPUBindGroupDescriptor uniformBgDesc = {};
  uniformBgDesc.nextInChain = nullptr;
  uniformBgDesc.label = "Uniform Bind Group";
  uniformBgDesc.layout = uniformBindGroupLayout;
  uniformBgDesc.entryCount = 1;
  uniformBgDesc.entries = &uniformBgEntry;
  WGPUBindGroup uniformBindGroup =
      wgpuDeviceCreateBindGroup(device, &uniformBgDesc);

  // ── 13. Tworzenie Texture Bind Group Layout i Bind Group (group 1) ──
  WGPUBindGroupLayoutEntry textureBglEntries[2] = {};

  // @binding(0) texture
  textureBglEntries[0].nextInChain = nullptr;
  textureBglEntries[0].binding = 0;
  textureBglEntries[0].visibility = WGPUShaderStage_Fragment;
  textureBglEntries[0].texture.nextInChain = nullptr;
  textureBglEntries[0].texture.sampleType = WGPUTextureSampleType_Float;
  textureBglEntries[0].texture.viewDimension = WGPUTextureViewDimension_2D;
  textureBglEntries[0].texture.multisampled = false;
  textureBglEntries[0].buffer.type = WGPUBufferBindingType_Undefined;
  textureBglEntries[0].sampler.type = WGPUSamplerBindingType_Undefined;
  textureBglEntries[0].storageTexture.access =
      WGPUStorageTextureAccess_Undefined;

  // @binding(1) sampler
  textureBglEntries[1].nextInChain = nullptr;
  textureBglEntries[1].binding = 1;
  textureBglEntries[1].visibility = WGPUShaderStage_Fragment;
  textureBglEntries[1].sampler.nextInChain = nullptr;
  textureBglEntries[1].sampler.type = WGPUSamplerBindingType_Filtering;
  textureBglEntries[1].buffer.type = WGPUBufferBindingType_Undefined;
  textureBglEntries[1].texture.sampleType = WGPUTextureSampleType_Undefined;
  textureBglEntries[1].storageTexture.access =
      WGPUStorageTextureAccess_Undefined;

  WGPUBindGroupLayoutDescriptor textureBglDesc = {};
  textureBglDesc.nextInChain = nullptr;
  textureBglDesc.label = "Texture Bind Group Layout";
  textureBglDesc.entryCount = 2;
  textureBglDesc.entries = textureBglEntries;
  WGPUBindGroupLayout textureBindGroupLayout =
      wgpuDeviceCreateBindGroupLayout(device, &textureBglDesc);

  // Bind group entries
  WGPUBindGroupEntry textureBgEntries[2] = {};
  textureBgEntries[0].nextInChain = nullptr;
  textureBgEntries[0].binding = 0;
  textureBgEntries[0].textureView = playerTexture.view;
  textureBgEntries[0].sampler = nullptr;
  textureBgEntries[0].buffer = nullptr;

  textureBgEntries[1].nextInChain = nullptr;
  textureBgEntries[1].binding = 1;
  textureBgEntries[1].sampler = playerTexture.sampler;
  textureBgEntries[1].textureView = nullptr;
  textureBgEntries[1].buffer = nullptr;

  WGPUBindGroupDescriptor textureBgDesc = {};
  textureBgDesc.nextInChain = nullptr;
  textureBgDesc.label = "Texture Bind Group";
  textureBgDesc.layout = textureBindGroupLayout;
  textureBgDesc.entryCount = 2;
  textureBgDesc.entries = textureBgEntries;
  WGPUBindGroup textureBindGroup =
      wgpuDeviceCreateBindGroup(device, &textureBgDesc);

  std::cout << "Bind groups created." << std::endl;

  // ── 14. Tworzenie Render Pipeline ────────────────────────
  WGPURenderPipeline pipeline =
      createPipeline(device, WGPUTextureFormat_BGRA8Unorm,
                     uniformBindGroupLayout, textureBindGroupLayout);
  if (!pipeline) {
    releaseTexture(playerTexture);
    wgpuBindGroupRelease(textureBindGroup);
    wgpuBindGroupLayoutRelease(textureBindGroupLayout);
    wgpuBindGroupRelease(uniformBindGroup);
    wgpuBindGroupLayoutRelease(uniformBindGroupLayout);
    wgpuBufferRelease(uniformBuffer);
    wgpuBufferRelease(indexBuffer);
    wgpuBufferRelease(vertexBuffer);
    wgpuSurfaceUnconfigure(surface);
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuSurfaceRelease(surface);
    wgpuAdapterRelease(adapter);
    wgpuInstanceRelease(instance);
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }

  // ── 15. Pętla renderowania (Textured Quad + WASD) ────────
  std::cout
      << "\nWarpEngine started! Use WASD to move. Rendering textured quad..."
      << std::endl;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // ── Obsługa klawiatury (WASD) ──────────────────────────
    const float speed = 5.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      playerUniforms.position[1] -= speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      playerUniforms.position[1] += speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      playerUniforms.position[0] -= speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      playerUniforms.position[0] += speed;

    // Prześlij zaktualizowaną pozycję do GPU
    wgpuQueueWriteBuffer(queue, uniformBuffer, 0, &playerUniforms,
                         sizeof(playerUniforms));

    // Pobierz bieżącą teksturę z surface
    WGPUSurfaceTexture surfaceTexture = {};
    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);

    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
      std::cerr << "Failed to get current surface texture!" << std::endl;
      break;
    }

    // Stwórz widok tekstury
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

    // Stwórz command encoder
    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.nextInChain = nullptr;
    encoderDesc.label = "Command Encoder";
    WGPUCommandEncoder encoder =
        wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

    // Rozpocznij render pass — czyszczenie kolorem granatowym
    WGPURenderPassColorAttachment colorAttachment = {};
    colorAttachment.nextInChain = nullptr;
    colorAttachment.view = textureView;
    colorAttachment.resolveTarget = nullptr;
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    colorAttachment.clearValue = WGPUColor{0.05, 0.05, 0.2, 1.0};

    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.nextInChain = nullptr;
    renderPassDesc.label = "Main Render Pass";
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &colorAttachment;
    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.occlusionQuerySet = nullptr;
    renderPassDesc.timestampWrites = nullptr;

    WGPURenderPassEncoder renderPass =
        wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

    // Ustaw pipeline, bind groups, bufory i rysuj quad
    wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);
    wgpuRenderPassEncoderSetBindGroup(renderPass, 0, uniformBindGroup, 0,
                                      nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPass, 1, textureBindGroup, 0,
                                      nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer, 0,
                                         sizeof(quadVertices));
    wgpuRenderPassEncoderSetIndexBuffer(renderPass, indexBuffer,
                                        WGPUIndexFormat_Uint16, 0,
                                        sizeof(quadIndices));
    wgpuRenderPassEncoderDrawIndexed(renderPass, 6, 1, 0, 0, 0);

    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);

    // Zakończ komendę i wyślij do kolejki
    WGPUCommandBufferDescriptor cmdBufDesc = {};
    cmdBufDesc.nextInChain = nullptr;
    cmdBufDesc.label = "Render Command Buffer";
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(encoder, &cmdBufDesc);

    wgpuQueueSubmit(queue, 1, &cmdBuf);

    // Prezentuj na ekranie
    wgpuSurfacePresent(surface);

    // Zwolnij zasoby tego frame'a
    wgpuCommandBufferRelease(cmdBuf);
    wgpuCommandEncoderRelease(encoder);
    wgpuTextureViewRelease(textureView);
    wgpuTextureRelease(surfaceTexture.texture);
  }

  // ── 16. Sprzątanie zasobów ───────────────────────────────
  std::cout << "\nShutting down WarpEngine..." << std::endl;

  wgpuRenderPipelineRelease(pipeline);
  wgpuBindGroupRelease(textureBindGroup);
  wgpuBindGroupLayoutRelease(textureBindGroupLayout);
  wgpuBindGroupRelease(uniformBindGroup);
  wgpuBindGroupLayoutRelease(uniformBindGroupLayout);
  wgpuBufferRelease(uniformBuffer);
  wgpuBufferRelease(indexBuffer);
  wgpuBufferRelease(vertexBuffer);
  releaseTexture(playerTexture);
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
