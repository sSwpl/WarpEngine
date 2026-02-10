#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

#include "wgpu_surface.h"

// ============================================================
//  Struktury danych
// ============================================================

// Uniform buffer przesyłany do GPU — pozycja gracza (vec2f + padding do 16B)
struct PlayerUniforms {
  float position[2]; // x, y
  float _pad[2];     // wyrównanie do 16 bajtów (std140)
};

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
//  WGSL Shader
// ============================================================

const char *shaderSource = R"(
struct PlayerUniforms {
    position: vec2f,
};

@group(0) @binding(0) var<uniform> player: PlayerUniforms;

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f,
};

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var pos = array<vec2f, 3>(
        vec2f( 0.0,  0.5),   // góra
        vec2f(-0.5, -0.5),   // lewy dół
        vec2f( 0.5, -0.5)    // prawy dół
    );

    var out: VertexOutput;
    out.position = vec4f(pos[vertexIndex] + player.position, 0.0, 1.0);
    out.color = vec3f(1.0, 0.0, 0.0); // czerwony
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    return vec4f(in.color, 1.0);
}
)";

// ============================================================
//  Pipeline Creation
// ============================================================

WGPURenderPipeline createPipeline(WGPUDevice device,
                                  WGPUTextureFormat surfaceFormat,
                                  WGPUBindGroupLayout bindGroupLayout) {
  // 1. Shader module z kodu WGSL
  WGPUShaderModuleWGSLDescriptor wgslDesc = {};
  wgslDesc.chain.next = nullptr;
  wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
  wgslDesc.code = shaderSource;

  WGPUShaderModuleDescriptor shaderDesc = {};
  shaderDesc.nextInChain = &wgslDesc.chain;
  shaderDesc.label = "Triangle Shader";
  shaderDesc.hintCount = 0;
  shaderDesc.hints = nullptr;

  WGPUShaderModule shaderModule =
      wgpuDeviceCreateShaderModule(device, &shaderDesc);
  if (!shaderModule) {
    std::cerr << "Failed to create shader module!" << std::endl;
    return nullptr;
  }

  // 2. Tworzenie explicit pipeline layout z bind group layout
  WGPUPipelineLayoutDescriptor layoutDesc = {};
  layoutDesc.nextInChain = nullptr;
  layoutDesc.label = "Pipeline Layout";
  layoutDesc.bindGroupLayoutCount = 1;
  layoutDesc.bindGroupLayouts = &bindGroupLayout;

  WGPUPipelineLayout pipelineLayout =
      wgpuDeviceCreatePipelineLayout(device, &layoutDesc);

  // 3. Color target state (format musi pasować do surface)
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

  // 4. Fragment state
  WGPUFragmentState fragmentState = {};
  fragmentState.nextInChain = nullptr;
  fragmentState.module = shaderModule;
  fragmentState.entryPoint = "fs_main";
  fragmentState.constantCount = 0;
  fragmentState.constants = nullptr;
  fragmentState.targetCount = 1;
  fragmentState.targets = &colorTarget;

  // 5. Pipeline descriptor
  WGPURenderPipelineDescriptor pipelineDesc = {};
  pipelineDesc.nextInChain = nullptr;
  pipelineDesc.label = "Triangle Pipeline";
  pipelineDesc.layout = pipelineLayout;

  // Vertex state (brak buforów — pozycje hardkodowane w shaderze)
  pipelineDesc.vertex.nextInChain = nullptr;
  pipelineDesc.vertex.module = shaderModule;
  pipelineDesc.vertex.entryPoint = "vs_main";
  pipelineDesc.vertex.constantCount = 0;
  pipelineDesc.vertex.constants = nullptr;
  pipelineDesc.vertex.bufferCount = 0;
  pipelineDesc.vertex.buffers = nullptr;

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

  // ── 9. Tworzenie Uniform Buffer i Bind Group ─────────────
  PlayerUniforms playerUniforms = {};
  playerUniforms.position[0] = 0.0f;
  playerUniforms.position[1] = 0.0f;
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

  // Bind group layout (jeden wpis: buffer uniform, widoczny w vertex shader)
  WGPUBindGroupLayoutEntry bglEntry = {};
  bglEntry.nextInChain = nullptr;
  bglEntry.binding = 0;
  bglEntry.visibility = WGPUShaderStage_Vertex;
  bglEntry.buffer.nextInChain = nullptr;
  bglEntry.buffer.type = WGPUBufferBindingType_Uniform;
  bglEntry.buffer.hasDynamicOffset = false;
  bglEntry.buffer.minBindingSize = sizeof(PlayerUniforms);
  // Zeruj inne typy bindingów
  bglEntry.sampler.type = WGPUSamplerBindingType_Undefined;
  bglEntry.texture.sampleType = WGPUTextureSampleType_Undefined;
  bglEntry.storageTexture.access = WGPUStorageTextureAccess_Undefined;

  WGPUBindGroupLayoutDescriptor bglDesc = {};
  bglDesc.nextInChain = nullptr;
  bglDesc.label = "Player Bind Group Layout";
  bglDesc.entryCount = 1;
  bglDesc.entries = &bglEntry;
  WGPUBindGroupLayout bindGroupLayout =
      wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

  // Bind group — łączy bufor z layoutem
  WGPUBindGroupEntry bgEntry = {};
  bgEntry.nextInChain = nullptr;
  bgEntry.binding = 0;
  bgEntry.buffer = uniformBuffer;
  bgEntry.offset = 0;
  bgEntry.size = sizeof(PlayerUniforms);
  bgEntry.sampler = nullptr;
  bgEntry.textureView = nullptr;

  WGPUBindGroupDescriptor bgDesc = {};
  bgDesc.nextInChain = nullptr;
  bgDesc.label = "Player Bind Group";
  bgDesc.layout = bindGroupLayout;
  bgDesc.entryCount = 1;
  bgDesc.entries = &bgEntry;
  WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);

  // ── 10. Tworzenie Render Pipeline ────────────────────────
  WGPURenderPipeline pipeline =
      createPipeline(device, WGPUTextureFormat_BGRA8Unorm, bindGroupLayout);
  if (!pipeline) {
    wgpuBindGroupRelease(bindGroup);
    wgpuBindGroupLayoutRelease(bindGroupLayout);
    wgpuBufferRelease(uniformBuffer);
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

  // ── 11. Pętla renderowania (Triangle + WASD) ─────────────
  std::cout << "\nWarpEngine started! Use WASD to move. Rendering..."
            << std::endl;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // ── Obsługa klawiatury (WASD) ──────────────────────────
    const float speed = 0.05f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      playerUniforms.position[1] += speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      playerUniforms.position[1] -= speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      playerUniforms.position[0] -= speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      playerUniforms.position[0] += speed;

    // Prześlij zaktualizowaną pozycję do GPU
    wgpuQueueWriteBuffer(queue, uniformBuffer, 0, &playerUniforms,
                         sizeof(playerUniforms));

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

    // Ustaw pipeline i bind group, rysuj trójkąt
    wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);
    wgpuRenderPassEncoderSetBindGroup(renderPass, 0, bindGroup, 0, nullptr);
    wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);

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

  // ── 12. Sprzątanie zasobów ───────────────────────────────
  std::cout << "\nShutting down WarpEngine..." << std::endl;

  wgpuRenderPipelineRelease(pipeline);
  wgpuBindGroupRelease(bindGroup);
  wgpuBindGroupLayoutRelease(bindGroupLayout);
  wgpuBufferRelease(uniformBuffer);
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
