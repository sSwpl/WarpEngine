#include "game.h"
#include "wgpu_surface.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <random>


// Forward declarations mechanisms
void onAdapterRequestEnded(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                           char const *message, void *userdata);
void onDeviceRequestEnded(WGPURequestDeviceStatus status, WGPUDevice device,
                          char const *message, void *userdata);
void onUncapturedError(WGPUErrorType type, char const *message, void *userdata);
void onDeviceLost(WGPUDeviceLostReason reason, char const *message,
                  void *userdata);

// Updated Shader with Color Tint
const char *shaderSourceWGSL = R"(
struct CameraUniforms {
    viewProj: mat4x4<f32>,
};

@group(0) @binding(0) var<uniform> camera: CameraUniforms;
@group(1) @binding(0) var spriteTex: texture_2d<f32>;
@group(1) @binding(1) var spriteSampler: sampler;

struct VertexInput {
    @location(0) position: vec2f,
    @location(1) uv: vec2f,
};

struct InstanceInput {
    @location(2) instPos: vec2f,
    @location(3) instScale: vec2f,
    @location(4) uvOffset: vec2f,
    @location(5) uvScale: vec2f,
    @location(6) color: vec4f, // New Color Attribute
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) color: vec4f,
};

@vertex
fn vs_main(in: VertexInput, inst: InstanceInput) -> VertexOutput {
    var out: VertexOutput;
    let worldPos = (in.position * inst.instScale) + inst.instPos;
    out.position = camera.viewProj * vec4f(worldPos, 0.0, 1.0);
    out.uv = (in.uv * inst.uvScale) + inst.uvOffset;
    out.color = inst.color;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let texColor = textureSample(spriteTex, spriteSampler, in.uv);
    if (texColor.a < 0.1) { discard; }
    return texColor * in.color; // Apply Tint
}
)";

Game::Game() {}
Game::~Game() { Cleanup(); }

bool Game::Initialize() {
  if (!glfwInit())
    return false;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window = glfwCreateWindow(width, height, "WarpEngine | Combat Update",
                            nullptr, nullptr);
  if (!window)
    return false;

  instance = wgpuCreateInstance(nullptr);
  surface = createSurfaceForWindow(instance, window);

  WGPURequestAdapterOptions adapterOpts = {};
  adapterOpts.compatibleSurface = surface;
  adapterOpts.powerPreference = WGPUPowerPreference_HighPerformance;
  wgpuInstanceRequestAdapter(instance, &adapterOpts, onAdapterRequestEnded,
                             &adapter);

  WGPUDeviceDescriptor deviceDesc = {};
  deviceDesc.deviceLostCallback = onDeviceLost;
  wgpuAdapterRequestDevice(adapter, &deviceDesc, onDeviceRequestEnded, &device);
  wgpuDeviceSetUncapturedErrorCallback(device, onUncapturedError, nullptr);
  queue = wgpuDeviceGetQueue(device);

  surfConfig.device = device;
  surfConfig.format = WGPUTextureFormat_BGRA8Unorm;
  surfConfig.usage = WGPUTextureUsage_RenderAttachment;
  surfConfig.width = width;
  surfConfig.height = height;
  surfConfig.presentMode = WGPUPresentMode_Fifo;
  wgpuSurfaceConfigure(surface, &surfConfig);

  InitGraphics();
  InitGame();

  return true;
}

void Game::InitGraphics() {
  atlasTexture = loadTexture(device, queue, "assets/atlas.png");

  struct Vertex {
    float x, y, u, v;
  };
  Vertex quadVertices[4] = {
      {-0.5f, -0.5f, 0.0f, 0.0f},
      {0.5f, -0.5f, 1.0f, 0.0f},
      {0.5f, 0.5f, 1.0f, 1.0f},
      {-0.5f, 0.5f, 0.0f, 1.0f},
  };

  WGPUBufferDescriptor vBufDesc = {};
  vBufDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
  vBufDesc.size = sizeof(quadVertices);
  vertexBuffer = wgpuDeviceCreateBuffer(device, &vBufDesc);
  wgpuQueueWriteBuffer(queue, vertexBuffer, 0, quadVertices,
                       sizeof(quadVertices));

  uint16_t quadIndices[6] = {0, 1, 2, 0, 2, 3};
  WGPUBufferDescriptor iBufDesc = {};
  iBufDesc.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
  iBufDesc.size = sizeof(quadIndices);
  indexBuffer = wgpuDeviceCreateBuffer(device, &iBufDesc);
  wgpuQueueWriteBuffer(queue, indexBuffer, 0, quadIndices, sizeof(quadIndices));

  WGPUBufferDescriptor instBufDesc = {};
  instBufDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
  instBufDesc.size = 20000 * sizeof(InstanceData);
  instanceBuffer = wgpuDeviceCreateBuffer(device, &instBufDesc);

  WGPUBufferDescriptor uBufDesc = {};
  uBufDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
  uBufDesc.size = sizeof(CameraUniforms);
  uniformBuffer = wgpuDeviceCreateBuffer(device, &uBufDesc);

  WGPUShaderModuleWGSLDescriptor wgslDesc = {};
  wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
  wgslDesc.code = shaderSourceWGSL;
  WGPUShaderModuleDescriptor shaderDesc = {};
  shaderDesc.nextInChain = &wgslDesc.chain;
  WGPUShaderModule shaderModule =
      wgpuDeviceCreateShaderModule(device, &shaderDesc);

  // Bind Groups (same)
  WGPUBindGroupLayoutEntry camEntry = {};
  camEntry.binding = 0;
  camEntry.visibility = WGPUShaderStage_Vertex;
  camEntry.buffer.type = WGPUBufferBindingType_Uniform;
  camEntry.buffer.minBindingSize = sizeof(CameraUniforms);
  WGPUBindGroupLayoutDescriptor camLayoutDesc = {};
  camLayoutDesc.entryCount = 1;
  camLayoutDesc.entries = &camEntry;
  WGPUBindGroupLayout camBindGroupLayout =
      wgpuDeviceCreateBindGroupLayout(device, &camLayoutDesc);

  WGPUBindGroupEntry camBgEntry = {};
  camBgEntry.binding = 0;
  camBgEntry.buffer = uniformBuffer;
  camBgEntry.size = sizeof(CameraUniforms);
  WGPUBindGroupDescriptor camBgDesc = {};
  camBgDesc.layout = camBindGroupLayout;
  camBgDesc.entryCount = 1;
  camBgDesc.entries = &camBgEntry;
  camBindGroup = wgpuDeviceCreateBindGroup(device, &camBgDesc);

  WGPUBindGroupLayoutEntry texEntries[2] = {};
  texEntries[0].binding = 0;
  texEntries[0].visibility = WGPUShaderStage_Fragment;
  texEntries[0].texture.sampleType = WGPUTextureSampleType_Float;
  texEntries[0].texture.viewDimension = WGPUTextureViewDimension_2D;
  texEntries[1].binding = 1;
  texEntries[1].visibility = WGPUShaderStage_Fragment;
  texEntries[1].sampler.type = WGPUSamplerBindingType_Filtering;

  WGPUBindGroupLayoutDescriptor texLayoutDesc = {};
  texLayoutDesc.entryCount = 2;
  texLayoutDesc.entries = texEntries;
  WGPUBindGroupLayout texBindGroupLayout =
      wgpuDeviceCreateBindGroupLayout(device, &texLayoutDesc);

  WGPUBindGroupEntry texBgEntries[2] = {};
  texBgEntries[0].binding = 0;
  texBgEntries[0].textureView = atlasTexture.view;
  texBgEntries[1].binding = 1;
  texBgEntries[1].sampler = atlasTexture.sampler;
  WGPUBindGroupDescriptor texBgDesc = {};
  texBgDesc.layout = texBindGroupLayout;
  texBgDesc.entryCount = 2;
  texBgDesc.entries = texBgEntries;
  texBindGroup = wgpuDeviceCreateBindGroup(device, &texBgDesc);

  WGPUBindGroupLayout layouts[] = {camBindGroupLayout, texBindGroupLayout};
  WGPUPipelineLayoutDescriptor pipeLayoutDesc = {};
  pipeLayoutDesc.bindGroupLayoutCount = 2;
  pipeLayoutDesc.bindGroupLayouts = layouts;
  WGPUPipelineLayout pipelineLayout =
      wgpuDeviceCreatePipelineLayout(device, &pipeLayoutDesc);

  WGPUVertexAttribute vertAttribs[2];
  vertAttribs[0].format = WGPUVertexFormat_Float32x2;
  vertAttribs[0].offset = 0;
  vertAttribs[0].shaderLocation = 0;
  vertAttribs[1].format = WGPUVertexFormat_Float32x2;
  vertAttribs[1].offset = 8;
  vertAttribs[1].shaderLocation = 1;
  WGPUVertexBufferLayout vertLayout = {};
  vertLayout.arrayStride = 16;
  vertLayout.stepMode = WGPUVertexStepMode_Vertex;
  vertLayout.attributeCount = 2;
  vertLayout.attributes = vertAttribs;

  // Instance Layout - Added Color!
  WGPUVertexAttribute instAttribs[5];
  instAttribs[0].format = WGPUVertexFormat_Float32x2;
  instAttribs[0].offset = 0;
  instAttribs[0].shaderLocation = 2;
  instAttribs[1].format = WGPUVertexFormat_Float32x2;
  instAttribs[1].offset = 8;
  instAttribs[1].shaderLocation = 3;
  instAttribs[2].format = WGPUVertexFormat_Float32x2;
  instAttribs[2].offset = 16;
  instAttribs[2].shaderLocation = 4;
  instAttribs[3].format = WGPUVertexFormat_Float32x2;
  instAttribs[3].offset = 24;
  instAttribs[3].shaderLocation = 5;
  instAttribs[4].format = WGPUVertexFormat_Float32x4;
  instAttribs[4].offset = 32;
  instAttribs[4].shaderLocation = 6; // Color

  WGPUVertexBufferLayout instLayout = {};
  instLayout.arrayStride = sizeof(InstanceData);
  instLayout.stepMode = WGPUVertexStepMode_Instance;
  instLayout.attributeCount = 5;
  instLayout.attributes = instAttribs;

  WGPUVertexBufferLayout bufLayouts[] = {vertLayout, instLayout};

  // Blend State - Alpha Blending
  WGPUBlendState blend = {};
  blend.color = {WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha,
                 WGPUBlendFactor_OneMinusSrcAlpha};
  blend.alpha = {WGPUBlendOperation_Add, WGPUBlendFactor_One,
                 WGPUBlendFactor_Zero};
  WGPUColorTargetState colorTarget = {};
  colorTarget.format = WGPUTextureFormat_BGRA8Unorm;
  colorTarget.blend = &blend;
  colorTarget.writeMask = WGPUColorWriteMask_All;
  WGPUFragmentState frag = {};
  frag.module = shaderModule;
  frag.entryPoint = "fs_main";
  frag.targetCount = 1;
  frag.targets = &colorTarget;

  WGPURenderPipelineDescriptor pipeDesc = {};
  pipeDesc.layout = pipelineLayout;
  pipeDesc.vertex.module = shaderModule;
  pipeDesc.vertex.entryPoint = "vs_main";
  pipeDesc.vertex.bufferCount = 2;
  pipeDesc.vertex.buffers = bufLayouts;
  pipeDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  pipeDesc.primitive.frontFace = WGPUFrontFace_CCW;
  pipeDesc.primitive.cullMode = WGPUCullMode_None;
  pipeDesc.fragment = &frag;
  pipeDesc.multisample.count = 1;
  pipeDesc.multisample.mask = ~0u;

  pipeline = wgpuDeviceCreateRenderPipeline(device, &pipeDesc);
  wgpuShaderModuleRelease(shaderModule);
  wgpuPipelineLayoutRelease(pipelineLayout);
}

void Game::InitGame() {
  entities.reserve(20000);

  // 1. Player
  entities.push_back({});
  player = &entities.back();
  player->type = EntityType::Player;
  player->position = {0, 0};
  player->scale = {64, 64};
  player->uvOffset = {0.0f, 0.0f};
  player->uvScale = {0.5f, 0.5f};
  player->radius = 20.0f;
  player->color = {1.0f, 1.0f, 1.0f, 1.0f};
  player->hp = 100.0f;

  // 2. Initial Crystals
  std::mt19937 rng(12345);
  std::uniform_real_distribution<float> distPos(-1000.0f, 1000.0f);

  for (int i = 0; i < 30; ++i) {
    Entity e;
    e.type = EntityType::Crystal;
    e.position = {distPos(rng), distPos(rng)};
    e.scale = {64, 64};
    e.uvOffset = {0.5f, 0.5f};
    e.uvScale = {0.5f, 0.5f};
    e.radius = 15.0f;
    e.color = {0.5f, 1.0f, 1.0f, 1.0f}; // Cyan tint
    entities.push_back(e);
  }
}

void Game::SpawnEnemy() {
  std::random_device rd;
  std::mt19937 rng(rd());

  std::uniform_real_distribution<float> distAngle(0.0f, 6.28318f);
  float angle = distAngle(rng);
  std::uniform_real_distribution<float> distR(900.0f, 1300.0f);
  float r = distR(rng);

  glm::vec2 spawnPos =
      player->position + glm::vec2(cos(angle) * r, sin(angle) * r);

  Entity e;
  e.position = spawnPos;
  e.scale = {64, 64};
  e.uvScale = {0.5f, 0.5f};
  e.radius = 25.0f;
  e.hp = 30.0f;

  std::bernoulli_distribution distType(0.5);
  if (distType(rng)) {
    e.type = EntityType::Blob;
    e.uvOffset = {0.5f, 0.0f};
    e.color = {0.8f, 1.0f, 0.8f, 1.0f}; // Greenish
  } else {
    e.type = EntityType::Skeleton;
    e.uvOffset = {0.0f, 0.5f};
    e.color = {1.0f, 0.9f, 0.9f, 1.0f}; // Pale Reddish
  }

  entities.push_back(e);
}

void Game::SpawnBullet(glm::vec2 targetPos) {
  if (player == nullptr)
    return;

  Entity b;
  b.type = EntityType::Bullet;
  b.position = player->position;
  b.scale = {32, 32};
  b.uvOffset = {0.5f, 0.5f}; // Re-using Crystal sprite for bullet for now
                             // (looks like projectile)
  b.uvScale = {0.5f, 0.5f};
  b.radius = 10.0f;
  b.color = {1.0f, 1.0f, 0.0f, 1.0f}; // Yellow/Gold
  b.lifeTime = 2.0f;
  b.damage = 15.0f;

  glm::vec2 dir = targetPos - player->position;
  if (glm::length(dir) > 0.1f) {
    b.velocity = glm::normalize(dir) * 600.0f; // Fast bullet
  } else {
    b.velocity = {600.0f, 0.0f};
  }

  entities.push_back(b);
}

int Game::FindNearestEnemy() {
  int nearest = -1;
  float minDistSq = 1000000.0f;

  for (size_t i = 1; i < entities.size(); ++i) {
    Entity &e = entities[i];
    if (e.type != EntityType::Blob && e.type != EntityType::Skeleton)
      continue;

    glm::vec2 d = player->position - e.position;
    float d2 = glm::dot(d, d);

    if (d2 < minDistSq && d2 < (800.0f * 800.0f)) { // Only within range
      minDistSq = d2;
      nearest = (int)i;
    }
  }
  return nearest;
}

void Game::ProcessInput(float dt) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  glm::vec2 input{0, 0};
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    input.y -= 1;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    input.y += 1;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    input.x -= 1;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    input.x += 1;

  if (glm::length(input) > 0) {
    player->position += glm::normalize(input) * PLAYER_SPEED * dt;
  }
}

void Game::Update(float dt) {
  ProcessInput(dt);
  gameTime += dt;
  spawnTimer += dt;
  fireTimer += dt;

  // Spawning
  float difficulty = 1.0f + (gameTime / 60.0f) * 10.0f;
  float spawnInterval = 0.5f / difficulty;
  if (spawnInterval < 0.05f)
    spawnInterval = 0.05f;

  if (spawnTimer >= spawnInterval && entities.size() < 15000) {
    spawnTimer = 0.0f;
    SpawnEnemy();
  }

  // Auto-Fire Weapon (0.2s cooldown)
  if (fireTimer >= 0.2f) {
    int targetIdx = FindNearestEnemy();
    if (targetIdx != -1) {
      SpawnBullet(entities[targetIdx].position);
      fireTimer = 0.0f;
    }
  }

  // Update Entities (Move Bullets & Enforce Lifetime)
  for (int i = (int)entities.size() - 1; i >= 1; --i) {
    Entity &e = entities[i];

    // Bullet Logic
    if (e.type == EntityType::Bullet) {
      e.position += e.velocity * dt;
      e.lifeTime -= dt;
      if (e.lifeTime <= 0.0f) {
        entities[i] = entities.back();
        entities.pop_back();
        continue;
      }

      // Bullet vs Enemy Collision
      // Brute force check against enemies? Or just verify nearest?
      // Checking against ALL enemies is slow for every bullet.
      // But we have spatial grid logic below, maybe handle it there?
      // For now, simpler check: optimize by checking only if close?
      // Let's rely on the O(M*N) for now (bullets * enemies). Bullets are few
      // (<50).
      bool hit = false;
      for (int j = 1; j < (int)entities.size(); ++j) { // Check forward (or all)
        Entity &tgt = entities[j];
        if (tgt.type == EntityType::Blob || tgt.type == EntityType::Skeleton) {
          if (glm::distance(e.position, tgt.position) <
              (tgt.radius + e.radius)) {
            // Hit!
            tgt.hp -= e.damage;
            hit = true;

            // Enemy Death
            if (tgt.hp <= 0.0f) {
              // Transform to Crystal (XP)
              tgt.type = EntityType::Crystal;
              tgt.color = {0.5f, 1.0f, 1.0f, 1.0f};
              tgt.uvOffset = {0.5f, 0.5f}; // Crystal sprite
              tgt.radius = 15.0f;
            }

            // Push back enemy slightly
            tgt.position += glm::normalize(e.velocity) * 10.0f;

            break; // Bullet hits one enemy
          }
        }
      }
      if (hit) {
        entities[i] = entities.back();
        entities.pop_back();
        continue;
      }
    }

    // Crystal Collection
    if (e.type == EntityType::Crystal) {
      if (glm::distance(player->position, e.position) <
          (player->radius + e.radius + 10.0f)) {
        score++;
        entities[i] = entities.back();
        entities.pop_back();
      }
    }
  }

  player = &entities[0]; // Refresh pointer

  // Camera
  float camX = player->position.x - width / 2.0f;
  float camY = player->position.y - height / 2.0f;
  glm::mat4 view =
      glm::translate(glm::mat4(1.0f), glm::vec3(-camX, -camY, 0.0f));
  glm::mat4 proj =
      glm::ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f);
  camUniforms.viewProj = proj * view;
  wgpuQueueWriteBuffer(queue, uniformBuffer, 0, &camUniforms,
                       sizeof(CameraUniforms));

  // Spatial Grid logic (Separation & Movement for Enemies)
  // Same as before...
  const float GRID_CELL_SIZE = 100.0f;
  const int GRID_W = 40;
  const int GRID_H = 40;
  const float WORLD_OFFSET = 2000.0f;
  static std::vector<int> grid[GRID_W][GRID_H];
  for (int x = 0; x < GRID_W; ++x)
    for (int y = 0; y < GRID_H; ++y)
      grid[x][y].clear();

  for (size_t i = 1; i < entities.size(); ++i) {
    Entity &e = entities[i];
    if (e.type == EntityType::Blob || e.type == EntityType::Skeleton) {
      glm::vec2 dir = player->position - e.position;
      float dist = glm::length(dir);
      if (dist > 30.0f) {
        e.position += glm::normalize(dir) * 100.0f * dt;
      }
      // Add to grid
      int gx = (int)((e.position.x + WORLD_OFFSET) / GRID_CELL_SIZE);
      int gy = (int)((e.position.y + WORLD_OFFSET) / GRID_CELL_SIZE);
      if (gx >= 0 && gx < GRID_W && gy >= 0 && gy < GRID_H)
        grid[gx][gy].push_back((int)i);
    }
  }

  // Resolve Separation
  for (size_t i = 1; i < entities.size(); ++i) {
    Entity &e = entities[i];
    if (e.type != EntityType::Blob && e.type != EntityType::Skeleton)
      continue;

    int gx = (int)((e.position.x + WORLD_OFFSET) / GRID_CELL_SIZE);
    int gy = (int)((e.position.y + WORLD_OFFSET) / GRID_CELL_SIZE);
    if (gx < 0 || gx >= GRID_W || gy < 0 || gy >= GRID_H)
      continue;

    for (int nx = gx - 1; nx <= gx + 1; ++nx) {
      for (int ny = gy - 1; ny <= gy + 1; ++ny) {
        if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H) {
          for (int otherId : grid[nx][ny]) {
            if (otherId == (int)i)
              continue;

            Entity &other = entities[otherId];
            if (other.type != EntityType::Blob &&
                other.type != EntityType::Skeleton)
              continue;

            glm::vec2 dir = e.position - other.position;
            float distSq = glm::dot(dir, dir);
            float minDist = e.radius + other.radius;
            float minDistSq = minDist * minDist;

            if (distSq < minDistSq && distSq > 0.001f) {
              float dist = std::sqrt(distSq);
              glm::vec2 push = (dir / dist) * (minDist - dist) * 0.5f;
              e.position += push;
            }
          }
        }
      }
    }
  }
}

void Game::Render() {
  WGPUSurfaceTexture surfaceTexture;
  wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
  if (!surfaceTexture.texture)
    return;

  WGPUTextureViewDescriptor viewDesc = {};
  viewDesc.baseMipLevel = 0;
  viewDesc.mipLevelCount = 1;
  viewDesc.baseArrayLayer = 0;
  viewDesc.arrayLayerCount = 1;
  viewDesc.format = WGPUTextureFormat_BGRA8Unorm;
  viewDesc.dimension = WGPUTextureViewDimension_2D;
  WGPUTextureView targetView =
      wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);

  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);

  WGPURenderPassColorAttachment colorAttach = {};
  colorAttach.view = targetView;
  colorAttach.loadOp = WGPULoadOp_Clear;
  colorAttach.storeOp = WGPUStoreOp_Store;
  colorAttach.clearValue = {0.05, 0.05, 0.1, 1.0};
  WGPURenderPassDescriptor passDesc = {};
  passDesc.colorAttachmentCount = 1;
  passDesc.colorAttachments = &colorAttach;

  static std::vector<InstanceData> instanceData;
  if (instanceData.size() != entities.size())
    instanceData.resize(entities.size());

  for (size_t i = 0; i < entities.size(); ++i) {
    instanceData[i].position = entities[i].position;
    instanceData[i].scale = entities[i].scale;
    instanceData[i].uvOffset = entities[i].uvOffset;
    instanceData[i].uvScale = entities[i].uvScale;
    instanceData[i].color = entities[i].color; // Color Copy!
  }
  wgpuQueueWriteBuffer(queue, instanceBuffer, 0, instanceData.data(),
                       instanceData.size() * sizeof(InstanceData));

  WGPURenderPassEncoder pass =
      wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
  wgpuRenderPassEncoderSetPipeline(pass, pipeline);
  wgpuRenderPassEncoderSetBindGroup(pass, 0, camBindGroup, 0, nullptr);
  wgpuRenderPassEncoderSetBindGroup(pass, 1, texBindGroup, 0, nullptr);
  wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertexBuffer, 0, 4 * 16);
  wgpuRenderPassEncoderSetVertexBuffer(
      pass, 1, instanceBuffer, 0, instanceData.size() * sizeof(InstanceData));
  wgpuRenderPassEncoderSetIndexBuffer(pass, indexBuffer, WGPUIndexFormat_Uint16,
                                      0, 12);
  wgpuRenderPassEncoderDrawIndexed(pass, 6, (uint32_t)entities.size(), 0, 0, 0);
  wgpuRenderPassEncoderEnd(pass);

  WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
  wgpuQueueSubmit(queue, 1, &cmd);
  wgpuSurfacePresent(surface);

  wgpuCommandBufferRelease(cmd);
  wgpuCommandEncoderRelease(encoder);
  wgpuRenderPassEncoderRelease(pass);
  wgpuTextureViewRelease(targetView);
  wgpuTextureRelease(surfaceTexture.texture);
}

void Game::Run() {
  float lastTime = (float)glfwGetTime();

  while (!glfwWindowShouldClose(window)) {
    float currentTime = (float)glfwGetTime();
    float dt = currentTime - lastTime;
    lastTime = currentTime;

    if (dt > 0.1f)
      dt = 0.1f;

    glfwPollEvents();
    Update(dt);
    Render();

    static float timer = 0.0f;
    static int frames = 0;
    timer += dt;
    frames++;
    if (timer >= 1.0f) {
      std::string title = "WarpEngine | FPS: " + std::to_string(frames) +
                          " | Entities: " + std::to_string(entities.size()) +
                          " | Score: " + std::to_string(score);
      glfwSetWindowTitle(window, title.c_str());
      timer = 0.0f;
      frames = 0;
    }
  }
}

void Game::Cleanup() {
  if (window)
    glfwDestroyWindow(window);
  glfwTerminate();
}

// Global Callbacks
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

void onUncapturedError(WGPUErrorType type, char const *message,
                       void * /*userdata*/) {
  std::cerr << "[WebGPU Error] type=" << type << ": "
            << (message ? message : "(no message)") << std::endl;
}

void onDeviceLost(WGPUDeviceLostReason reason, char const *message,
                  void * /*userdata*/) {
  std::cerr << "[WebGPU] Device lost! reason=" << reason << ": "
            << (message ? message : "(no message)") << std::endl;
}
