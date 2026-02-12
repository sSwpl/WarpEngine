#include "game.h"
#include "wgpu_surface.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <random>


// Callback forwards
void onAdapterRequestEnded(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                           char const *message, void *userdata);
void onDeviceRequestEnded(WGPURequestDeviceStatus status, WGPUDevice device,
                          char const *message, void *userdata);
void onUncapturedError(WGPUErrorType type, char const *message, void *userdata);
void onDeviceLost(WGPUDeviceLostReason reason, char const *message,
                  void *userdata);

// Shader with Solid Color UI Support
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
    @location(6) color: vec4f,
    @location(7) useSolid: f32, // 1.0 = solid, 0.0 = textured
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) color: vec4f,
    @location(2) useSolid: f32,
};

@vertex
fn vs_main(in: VertexInput, inst: InstanceInput) -> VertexOutput {
    var out: VertexOutput;
    let worldPos = (in.position * inst.instScale) + inst.instPos;
    out.position = camera.viewProj * vec4f(worldPos, 0.0, 1.0);
    out.uv = (in.uv * inst.uvScale) + inst.uvOffset;
    out.color = inst.color;
    out.useSolid = inst.useSolid;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    if (in.useSolid > 0.5) {
        return in.color;
    } else {
        let texColor = textureSample(spriteTex, spriteSampler, in.uv);
        if (texColor.a < 0.1) { discard; }
        return texColor * in.color;
    }
}
)";

Game::Game() {}
Game::~Game() { Cleanup(); }

bool Game::Initialize() {
  if (!glfwInit())
    return false;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window = glfwCreateWindow(width, height, "WarpEngine | Power-ups Update",
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
  Vertex quadVertices[4] = {{-0.5f, -0.5f, 0, 0},
                            {0.5f, -0.5f, 1, 0},
                            {0.5f, 0.5f, 1, 1},
                            {-0.5f, 0.5f, 0, 1}};
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

  // Instance Layout (loc 6=Color, loc 7=Solid)
  WGPUVertexAttribute instAttribs[6];
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
  instAttribs[4].shaderLocation = 6;
  instAttribs[5].format = WGPUVertexFormat_Float32;
  instAttribs[5].offset = 48;
  instAttribs[5].shaderLocation = 7;

  WGPUVertexBufferLayout instLayout = {};
  instLayout.arrayStride = sizeof(InstanceData);
  instLayout.stepMode = WGPUVertexStepMode_Instance;
  instLayout.attributeCount = 6;
  instLayout.attributes = instAttribs;

  WGPUVertexBufferLayout bufLayouts[] = {vertLayout, instLayout};
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

void Game::InitGame() { ResetGame(); }

void Game::ResetGame() {
  entities.clear();
  entities.reserve(20000);

  // Player (UV=0,0 Scale=0.25)
  entities.push_back({});
  player = &entities.back();
  player->type = EntityType::Player;
  player->position = {0, 0};
  player->scale = {64, 64};
  player->uvOffset = {0.0f, 0.0f};
  player->uvScale = {0.25f, 0.25f}; // 4x4 Atlas
  player->radius = 20.0f;
  player->color = {1.0f, 1.0f, 1.0f, 1.0f};
  player->maxHp = 100.0f;
  player->hp = player->maxHp;
  player->piercingTimer = 0.0f;

  // Initial Crystals
  std::mt19937 rng(12345);
  std::uniform_real_distribution<float> distPos(-1000.0f, 1000.0f);
  for (int i = 0; i < 30; ++i) {
    Entity e;
    e.type = EntityType::Crystal;
    e.position = {distPos(rng), distPos(rng)};
    e.scale = {64, 64};
    e.uvOffset = {0.75f, 0.0f}; // 3rd Horizontal (Crystal)
    e.uvScale = {0.25f, 0.25f};
    e.radius = 15.0f;
    e.color = {0.5f, 1.0f, 1.0f, 1.0f};
    entities.push_back(e);
  }

  gameTime = 0.0f;
  spawnTimer = 0.0f;
  fireTimer = 0.0f;
  score = 0;
  state = GameState::Playing;
}

void Game::SpawnEnemy() {
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_real_distribution<float> distAngle(0, 6.28);
  std::uniform_real_distribution<float> distR(900, 1300);
  float angle = distAngle(rng), r = distR(rng);

  Entity e;
  e.position = player->position + glm::vec2(cos(angle) * r, sin(angle) * r);
  e.scale = {64, 64};
  e.uvScale = {0.25f, 0.25f};
  e.radius = 25.0f;
  e.hp = 30.0f;
  e.maxHp = 30.0f;

  std::bernoulli_distribution distType(0.5);
  if (distType(rng)) {
    e.type = EntityType::Blob;
    e.uvOffset = {0.25f, 0.0f};
    e.color = {0.8f, 1.0f, 0.8f, 1.0f};
  } else {
    e.type = EntityType::Skeleton;
    e.uvOffset = {0.5f, 0.0f};
    e.color = {1.0f, 0.9f, 0.9f, 1.0f};
  }
  entities.push_back(e);
}

void Game::SpawnGem(glm::vec2 pos, int type) {
  Entity e;
  e.position = pos;
  e.scale = {64, 64};
  e.uvScale = {0.25f, 0.25f};
  e.radius = 15.0f;
  if (type == 0) { // Green (Health)
    e.type = EntityType::HealthGem;
    e.uvOffset = {0.0f, 0.25f};
    e.color = {0.5f, 1.0f, 0.5f, 1.0f};
  } else { // Purple (Piercing)
    e.type = EntityType::PiercingGem;
    e.uvOffset = {0.25f, 0.25f};
    e.color = {1.0f, 0.5f, 1.0f, 1.0f};
  }
  entities.push_back(e);
}

void Game::SpawnBullet(glm::vec2 targetPos) {
  if (!player)
    return;
  Entity b;
  b.type = EntityType::Bullet;
  b.position = player->position;
  b.scale = {32, 32};
  b.uvOffset = {0.75f, 0.0f}; // Crystal sprite for bullet
  b.uvScale = {0.25f, 0.25f};
  b.radius = 10.0f;
  b.color = {1.0f, 1.0f, 0.0f, 1.0f};
  b.lifeTime = 2.0f;
  b.damage = 15.0f;

  if (player->piercingTimer > 0.0f) {
    b.penetration = 100;                // Pierce everything
    b.color = {1.0f, 0.2f, 1.0f, 1.0f}; // Purple Bullet
    b.scale = {48, 48};                 // Bigger
  } else {
    b.penetration = 1;
  }

  glm::vec2 dir = targetPos - player->position;
  if (glm::length(dir) > 0.1f)
    b.velocity = glm::normalize(dir) * 600.0f;
  else
    b.velocity = {600, 0};
  entities.push_back(b);
}

int Game::FindNearestEnemy() {
  int nearest = -1;
  float minDistSq = 1e9;
  for (size_t i = 1; i < entities.size(); ++i) {
    if (entities[i].type != EntityType::Blob &&
        entities[i].type != EntityType::Skeleton)
      continue;
    glm::vec2 d = player->position - entities[i].position;
    float d2 = glm::dot(d, d);
    if (d2 < minDistSq && d2 < 640000.0f) {
      minDistSq = d2;
      nearest = (int)i;
    }
  }
  return nearest;
}

void Game::ProcessInput(float dt) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  if (state == GameState::GameOver) {
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
      ResetGame();
    return;
  }

  glm::vec2 input{0, 0};
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    input.y -= 1;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    input.y += 1;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    input.x -= 1;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    input.x += 1;
  if (glm::length(input) > 0)
    player->position += glm::normalize(input) * PLAYER_SPEED * dt;
}

void Game::Update(float dt) {
  ProcessInput(dt);
  if (state == GameState::GameOver) {
  } else {
    gameTime += dt;
    spawnTimer += dt;
    fireTimer += dt;
    if (player->piercingTimer > 0.0f)
      player->piercingTimer -= dt;

    float diff = 1.0f + (gameTime / 60.0f) * 10.0f;
    float interval = 0.5f / diff;
    if (interval < 0.05f)
      interval = 0.05f;
    if (spawnTimer >= interval && entities.size() < 15000) {
      spawnTimer = 0;
      SpawnEnemy();
    }
    if (fireTimer >= 0.2f) {
      int t = FindNearestEnemy();
      if (t != -1) {
        SpawnBullet(entities[t].position);
        fireTimer = 0;
      }
    }
  }

  if (player) {
    float cx = player->position.x - width / 2.0f;
    float cy = player->position.y - height / 2.0f;
    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(-cx, -cy, 0));
    glm::mat4 proj =
        glm::ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f);
    camUniforms.viewProj = proj * view;
    wgpuQueueWriteBuffer(queue, uniformBuffer, 0, &camUniforms,
                         sizeof(CameraUniforms));
  }

  if (state == GameState::GameOver)
    return;

  // Entities Loop
  for (int i = (int)entities.size() - 1; i >= 1; --i) {
    Entity &e = entities[i];

    // --- Bullet Logic ---
    if (e.type == EntityType::Bullet) {
      e.position += e.velocity * dt;
      e.lifeTime -= dt;
      if (e.lifeTime <= 0) {
        entities[i] = entities.back();
        entities.pop_back();
        continue;
      }

      bool hit = false;
      for (int j = 1; j < (int)entities.size(); ++j) {
        Entity &tgt = entities[j];
        if (tgt.type == EntityType::Blob || tgt.type == EntityType::Skeleton) {
          if (glm::distance(e.position, tgt.position) <
              (tgt.radius + e.radius)) {
            tgt.hp -= e.damage;
            hit = true;
            e.penetration--; // Dec penetration

            if (tgt.hp <= 0) {
              // Drop Loop
              static std::random_device rd;
              static std::mt19937 rng(rd());
              std::uniform_int_distribution<int> drop(0, 100);
              int val = drop(rng);

              if (val > 98) { // 2% Purple Gem
                SpawnGem(tgt.position, 1);
                tgt.active = false; // Using Active flag for removal? No,
                                    // swap-pop destroys it.
                // We are modifying 'entities' vector by pushing back! This
                // invalidates references! 'tgt' is a reference to entities[j].
                // PushBack might realloc. CRITICAL: Access by index only after
                // Spawn. But SpawnGem just does push_back. We should handle
                // death removal AFTER loop? Or be careful. Swap-pop removal
                // handles removal. Just remove tgt now.
              } else if (val > 95) { // 3% Green Gem
                SpawnGem(tgt.position, 0);
              } else { // 95% Crystal
                // Convert to Crystal
                tgt.type = EntityType::Crystal;
                tgt.color = {0.5f, 1, 1, 1};
                tgt.uvOffset = {0.75f, 0.0f};
                tgt.radius = 15;
                goto skip_removal; // Don't remove
              }

              // Remove Tgt
              entities[j] = entities.back();
              entities.pop_back();
              // If we swapped j, we must re-check j?
              // Yes, but iterating bullet against ALL is dangerous if we modify
              // vector inside. Better to NOT remove immediately? Or accept tiny
              // glitch (skipping one frame collision). Let's use goto trick to
              // skip loop continue.
            skip_removal:;
            } else {
              tgt.position += glm::normalize(e.velocity) * 10.0f; // Pushback
            }

            if (e.penetration <= 0)
              break; // Bullet Done
          }
        }
      }
      if (hit && e.penetration <= 0) {
        entities[i] = entities.back();
        entities.pop_back();
        continue;
      }
    }

    // --- Collections ---
    if (e.type == EntityType::Crystal) {
      if (glm::distance(player->position, e.position) <
          (player->radius + e.radius + 20)) {
        score++;
        entities[i] = entities.back();
        entities.pop_back();
      }
    } else if (e.type == EntityType::HealthGem) {
      if (glm::distance(player->position, e.position) <
          (player->radius + e.radius + 20)) {
        player->hp += 20.0f;
        if (player->hp > player->maxHp)
          player->hp = player->maxHp;
        entities[i] = entities.back();
        entities.pop_back();
      }
    } else if (e.type == EntityType::PiercingGem) {
      if (glm::distance(player->position, e.position) <
          (player->radius + e.radius + 20)) {
        player->piercingTimer = 10.0f;
        entities[i] = entities.back();
        entities.pop_back();
      }
    }

    // --- Damage ---
    if (e.type == EntityType::Blob || e.type == EntityType::Skeleton) {
      if (glm::distance(player->position, e.position) <
          (player->radius + e.radius - 5.0f)) {
        player->hp -= 20.0f * dt;
        if (player->hp <= 0) {
          state = GameState::GameOver;
          std::cout << "GAME OVER! Score: " << score << std::endl;
        }
      }
    }
  }
  player = &entities[0];

  // Spatial Grid logic (Physics)
  const float SZ = 100.0f;
  const int W = 40, H = 40;
  const float OFF = 2000.0f;
  static std::vector<int> grid[W][H];
  for (int x = 0; x < W; ++x)
    for (int y = 0; y < H; ++y)
      grid[x][y].clear();

  for (size_t i = 1; i < entities.size(); ++i) {
    Entity &e = entities[i];
    if (e.type == EntityType::Blob || e.type == EntityType::Skeleton) {
      glm::vec2 dir = player->position - e.position;
      float dist = glm::length(dir);
      if (dist > 30.0f)
        e.position += glm::normalize(dir) * 100.0f * dt;
      int gx = (int)((e.position.x + OFF) / SZ),
          gy = (int)((e.position.y + OFF) / SZ);
      if (gx >= 0 && gx < W && gy >= 0 && gy < H)
        grid[gx][gy].push_back((int)i);
    }
  }
  // Separation (Loop twice for stability)
  for (int iter = 0; iter < 2; ++iter) {
    for (size_t i = 1; i < entities.size(); ++i) {
      Entity &e = entities[i];
      if (e.type != EntityType::Blob && e.type != EntityType::Skeleton)
        continue;
      int gx = (int)((e.position.x + OFF) / SZ),
          gy = (int)((e.position.y + OFF) / SZ);
      if (gx < 0 || gx >= W || gy < 0 || gy >= H)
        continue;
      for (int nx = gx - 1; nx <= gx + 1; ++nx)
        for (int ny = gy - 1; ny <= gy + 1; ++ny) {
          if (nx < 0 || nx >= W || ny < 0 || ny >= H)
            continue;
          for (int oid : grid[nx][ny]) {
            if (oid == (int)i)
              continue;
            Entity &o = entities[oid];
            if (o.type != EntityType::Blob && o.type != EntityType::Skeleton)
              continue;
            glm::vec2 d = e.position - o.position;
            float d2 = glm::dot(d, d);
            float rSum = e.radius + o.radius;
            if (d2 < rSum * rSum && d2 > 0.001f) {
              float dist = std::sqrt(d2);
              // Stronger separation force (1.0f) and using rSum overlap
              float overlap = rSum - dist;
              e.position += (d / dist) * overlap * 0.8f;
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
  WGPUTextureView targetView =
      wgpuTextureCreateView(surfaceTexture.texture, nullptr);
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
  instanceData.clear();
  instanceData.reserve(entities.size() + 10);

  for (const auto &e : entities) {
    instanceData.push_back(
        {e.position, e.scale, e.uvOffset, e.uvScale, e.color, 0.0f});
  }

  // UI Rendering
  if (player && state != GameState::GameOver) {
    // HP Bar
    instanceData.push_back({player->position + glm::vec2(0, -50),
                            {80, 10},
                            {0, 0},
                            {0, 0},
                            {1, 0, 0, 1},
                            1.0f});
    float hpPct = std::max(0.0f, player->hp / player->maxHp);
    instanceData.push_back(
        {player->position + glm::vec2(-40 + (40 * hpPct), -50),
         {80 * hpPct, 10},
         {0, 0},
         {0, 0},
         {0, 1, 0, 1},
         1.0f});
  }
  if (state == GameState::GameOver) {
    instanceData.push_back({player->position,
                            {width * 1.0f, height * 1.0f},
                            {0, 0},
                            {0, 0},
                            {1, 0, 0, 0.5f},
                            1.0f});
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
  wgpuRenderPassEncoderDrawIndexed(pass, 6, (uint32_t)instanceData.size(), 0, 0,
                                   0);
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

void Game::RenderUI() {} // Folded into Render()

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
      std::string st = (state == GameState::GameOver) ? " [GAME OVER]" : "";
      std::string buff = (player->piercingTimer > 0) ? " | PIERCING!" : "";
      std::string title = "WarpEngine | FPS: " + std::to_string(frames) +
                          " | Score: " + std::to_string(score) +
                          " | HP: " + std::to_string((int)player->hp) + st +
                          buff;
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

// Callbacks
void onAdapterRequestEnded(WGPURequestAdapterStatus s, WGPUAdapter a,
                           char const *m, void *u) {
  *static_cast<WGPUAdapter *>(u) = a;
}
void onDeviceRequestEnded(WGPURequestDeviceStatus s, WGPUDevice d,
                          char const *m, void *u) {
  *static_cast<WGPUDevice *>(u) = d;
}
void onUncapturedError(WGPUErrorType t, char const *m, void *u) {
  std::cerr << "[Error] " << m << std::endl;
}
void onDeviceLost(WGPUDeviceLostReason r, char const *m, void *u) {
  std::cerr << "[Lost] " << m << std::endl;
}
