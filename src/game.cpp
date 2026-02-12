#include "game.h"
#include "wgpu_surface.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <random>

void onAdapterRequestEnded(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                           char const *message, void *userdata);
void onDeviceRequestEnded(WGPURequestDeviceStatus status, WGPUDevice device,
                          char const *message, void *userdata);
void onUncapturedError(WGPUErrorType type, char const *message, void *userdata);
void onDeviceLost(WGPUDeviceLostReason reason, char const *message,
                  void *userdata);

const char *shaderSourceWGSL = R"(
struct CameraUniforms { viewProj: mat4x4<f32> };
@group(0) @binding(0) var<uniform> camera: CameraUniforms;
@group(1) @binding(0) var spriteTex: texture_2d<f32>;
@group(1) @binding(1) var spriteSampler: sampler;
struct VertexInput { @location(0) position: vec2f, @location(1) uv: vec2f };
struct InstanceInput {
    @location(2) instPos: vec2f, @location(3) instScale: vec2f,
    @location(4) uvOffset: vec2f, @location(5) uvScale: vec2f,
    @location(6) color: vec4f, @location(7) useSolid: f32,
};
struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f, @location(1) color: vec4f, @location(2) useSolid: f32,
};
@vertex fn vs_main(in: VertexInput, inst: InstanceInput) -> VertexOutput {
    var out: VertexOutput;
    let worldPos = (in.position * inst.instScale) + inst.instPos;
    out.position = camera.viewProj * vec4f(worldPos, 0.0, 1.0);
    out.uv = (in.uv * inst.uvScale) + inst.uvOffset;
    out.color = inst.color; out.useSolid = inst.useSolid;
    return out;
}
@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    if (in.useSolid > 0.5) { return in.color; }
    let texColor = textureSample(spriteTex, spriteSampler, in.uv);
    if (texColor.a < 0.1) { discard; }
    return texColor * in.color;
}
)";

Game::Game() {}
Game::~Game() { Cleanup(); }

bool Game::Initialize() {
  if (!glfwInit())
    return false;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window = glfwCreateWindow(width, height, "WarpEngine | Survivor", nullptr,
                            nullptr);
  if (!window)
    return false;
  instance = wgpuCreateInstance(nullptr);
  surface = createSurfaceForWindow(instance, window);
  WGPURequestAdapterOptions ao = {};
  ao.compatibleSurface = surface;
  ao.powerPreference = WGPUPowerPreference_HighPerformance;
  wgpuInstanceRequestAdapter(instance, &ao, onAdapterRequestEnded, &adapter);
  WGPUDeviceDescriptor dd = {};
  dd.deviceLostCallback = onDeviceLost;
  wgpuAdapterRequestDevice(adapter, &dd, onDeviceRequestEnded, &device);
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
  audio.Init();
  return true;
}

void Game::InitGraphics() {
  atlasTexture = loadTexture(device, queue, "assets/atlas.png");
  fontTexture = loadTexture(device, queue, "assets/font.png");
  struct Vertex {
    float x, y, u, v;
  };
  Vertex qv[4] = {{-0.5f, -0.5f, 0, 0},
                  {0.5f, -0.5f, 1, 0},
                  {0.5f, 0.5f, 1, 1},
                  {-0.5f, 0.5f, 0, 1}};
  WGPUBufferDescriptor vd = {};
  vd.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
  vd.size = sizeof(qv);
  vertexBuffer = wgpuDeviceCreateBuffer(device, &vd);
  wgpuQueueWriteBuffer(queue, vertexBuffer, 0, qv, sizeof(qv));
  uint16_t ix[6] = {0, 1, 2, 0, 2, 3};
  WGPUBufferDescriptor id = {};
  id.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
  id.size = sizeof(ix);
  indexBuffer = wgpuDeviceCreateBuffer(device, &id);
  wgpuQueueWriteBuffer(queue, indexBuffer, 0, ix, sizeof(ix));
  WGPUBufferDescriptor ibd = {};
  ibd.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
  ibd.size = 25000 * sizeof(InstanceData);
  instanceBuffer = wgpuDeviceCreateBuffer(device, &ibd);
  WGPUBufferDescriptor ubd = {};
  ubd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
  ubd.size = sizeof(CameraUniforms);
  uniformBuffer = wgpuDeviceCreateBuffer(device, &ubd);
  WGPUShaderModuleWGSLDescriptor wd = {};
  wd.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
  wd.code = shaderSourceWGSL;
  WGPUShaderModuleDescriptor sd = {};
  sd.nextInChain = &wd.chain;
  WGPUShaderModule sm = wgpuDeviceCreateShaderModule(device, &sd);
  // Cam BGL & BG
  WGPUBindGroupLayoutEntry ce = {};
  ce.binding = 0;
  ce.visibility = WGPUShaderStage_Vertex;
  ce.buffer.type = WGPUBufferBindingType_Uniform;
  ce.buffer.minBindingSize = sizeof(CameraUniforms);
  WGPUBindGroupLayoutDescriptor cld = {};
  cld.entryCount = 1;
  cld.entries = &ce;
  WGPUBindGroupLayout cbl = wgpuDeviceCreateBindGroupLayout(device, &cld);
  WGPUBindGroupEntry cbe = {};
  cbe.binding = 0;
  cbe.buffer = uniformBuffer;
  cbe.size = sizeof(CameraUniforms);
  WGPUBindGroupDescriptor cbd = {};
  cbd.layout = cbl;
  cbd.entryCount = 1;
  cbd.entries = &cbe;
  camBindGroup = wgpuDeviceCreateBindGroup(device, &cbd);
  // Tex BGL
  WGPUBindGroupLayoutEntry te[2] = {};
  te[0].binding = 0;
  te[0].visibility = WGPUShaderStage_Fragment;
  te[0].texture.sampleType = WGPUTextureSampleType_Float;
  te[0].texture.viewDimension = WGPUTextureViewDimension_2D;
  te[1].binding = 1;
  te[1].visibility = WGPUShaderStage_Fragment;
  te[1].sampler.type = WGPUSamplerBindingType_Filtering;
  WGPUBindGroupLayoutDescriptor tld = {};
  tld.entryCount = 2;
  tld.entries = te;
  WGPUBindGroupLayout tbl = wgpuDeviceCreateBindGroupLayout(device, &tld);
  // Atlas BG
  WGPUBindGroupEntry abe[2] = {};
  abe[0].binding = 0;
  abe[0].textureView = atlasTexture.view;
  abe[1].binding = 1;
  abe[1].sampler = atlasTexture.sampler;
  WGPUBindGroupDescriptor abd = {};
  abd.layout = tbl;
  abd.entryCount = 2;
  abd.entries = abe;
  texBindGroup = wgpuDeviceCreateBindGroup(device, &abd);
  // Font BG
  WGPUBindGroupEntry fbe[2] = {};
  fbe[0].binding = 0;
  fbe[0].textureView = fontTexture.view;
  fbe[1].binding = 1;
  fbe[1].sampler = fontTexture.sampler;
  WGPUBindGroupDescriptor fbd = {};
  fbd.layout = tbl;
  fbd.entryCount = 2;
  fbd.entries = fbe;
  fontTexBindGroup = wgpuDeviceCreateBindGroup(device, &fbd);
  // Pipeline
  WGPUBindGroupLayout lays[] = {cbl, tbl};
  WGPUPipelineLayoutDescriptor pld = {};
  pld.bindGroupLayoutCount = 2;
  pld.bindGroupLayouts = lays;
  WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &pld);
  WGPUVertexAttribute va[2];
  va[0] = {WGPUVertexFormat_Float32x2, 0, 0};
  va[1] = {WGPUVertexFormat_Float32x2, 8, 1};
  WGPUVertexBufferLayout vl = {};
  vl.arrayStride = 16;
  vl.stepMode = WGPUVertexStepMode_Vertex;
  vl.attributeCount = 2;
  vl.attributes = va;
  WGPUVertexAttribute ia[6];
  ia[0] = {WGPUVertexFormat_Float32x2, 0, 2};
  ia[1] = {WGPUVertexFormat_Float32x2, 8, 3};
  ia[2] = {WGPUVertexFormat_Float32x2, 16, 4};
  ia[3] = {WGPUVertexFormat_Float32x2, 24, 5};
  ia[4] = {WGPUVertexFormat_Float32x4, 32, 6};
  ia[5] = {WGPUVertexFormat_Float32, 48, 7};
  WGPUVertexBufferLayout il = {};
  il.arrayStride = sizeof(InstanceData);
  il.stepMode = WGPUVertexStepMode_Instance;
  il.attributeCount = 6;
  il.attributes = ia;
  WGPUVertexBufferLayout bls[] = {vl, il};
  WGPUBlendState bl = {};
  bl.color = {WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha,
              WGPUBlendFactor_OneMinusSrcAlpha};
  bl.alpha = {WGPUBlendOperation_Add, WGPUBlendFactor_One,
              WGPUBlendFactor_Zero};
  WGPUColorTargetState ct = {};
  ct.format = WGPUTextureFormat_BGRA8Unorm;
  ct.blend = &bl;
  ct.writeMask = WGPUColorWriteMask_All;
  WGPUFragmentState fs = {};
  fs.module = sm;
  fs.entryPoint = "fs_main";
  fs.targetCount = 1;
  fs.targets = &ct;
  WGPURenderPipelineDescriptor pd = {};
  pd.layout = pl;
  pd.vertex.module = sm;
  pd.vertex.entryPoint = "vs_main";
  pd.vertex.bufferCount = 2;
  pd.vertex.buffers = bls;
  pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  pd.primitive.frontFace = WGPUFrontFace_CCW;
  pd.primitive.cullMode = WGPUCullMode_None;
  pd.fragment = &fs;
  pd.multisample.count = 1;
  pd.multisample.mask = ~0u;
  pipeline = wgpuDeviceCreateRenderPipeline(device, &pd);
  wgpuShaderModuleRelease(sm);
  wgpuPipelineLayoutRelease(pl);
}

void Game::InitGame() { ResetGame(); }

void Game::ResetGame() {
  entities.clear();
  entities.reserve(20000);
  entities.push_back({});
  player = &entities.back();
  player->type = EntityType::Player;
  player->position = {0, 0};
  player->scale = {64, 64};
  player->uvOffset = {0, 0};
  player->uvScale = {0.25f, 0.25f};
  player->radius = 20;
  player->color = {1, 1, 1, 1};
  player->maxHp = 100;
  player->hp = 100;
  player->piercingTimer = 0;
  std::mt19937 rng(12345);
  std::uniform_real_distribution<float> dp(-1000, 1000);
  for (int i = 0; i < 30; ++i) {
    Entity e;
    e.type = EntityType::Crystal;
    e.position = {dp(rng), dp(rng)};
    e.scale = {64, 64};
    e.uvOffset = {0.75f, 0};
    e.uvScale = {0.25f, 0.25f};
    e.radius = 15;
    e.color = {0.5f, 1, 1, 1};
    entities.push_back(e);
  }
  gameTime = 0;
  spawnTimer = 0;
  fireTimer = 0;
  score = 0;
  xp = 0;
  xpToNextLevel = 10;
  playerLevel = 0;
  playerSpeed = 300;
  bulletDamage = 15;
  bulletPenetration = 1;
  fireCooldown = 0.2f;
  nextBossTime = 60.0f;
  state = GameState::Playing;
}

void Game::SpawnEnemy() {
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_real_distribution<float> da(0, 6.28f), dr(900, 1300);
  float a = da(rng), r = dr(rng);
  Entity e;
  e.position = player->position + glm::vec2(cos(a) * r, sin(a) * r);
  e.scale = {64, 64};
  e.uvScale = {0.25f, 0.25f};
  e.radius = 25;
  if (std::bernoulli_distribution(0.5)(rng)) {
    // Blob: tanky, normal speed
    e.type = EntityType::Blob;
    e.uvOffset = {0.25f, 0};
    e.color = {0.8f, 1, 0.8f, 1};
    e.hp = 60;
    e.maxHp = 60;
    e.speed = 100;
  } else {
    // Skeleton: fragile, fast
    e.type = EntityType::Skeleton;
    e.uvOffset = {0.5f, 0};
    e.color = {1, 0.9f, 0.9f, 1};
    e.hp = 30;
    e.maxHp = 30;
    e.speed = 150;
  }
  entities.push_back(e);
}

void Game::SpawnBoss() {
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_real_distribution<float> da(0, 6.28f);
  float a = da(rng);
  Entity e;
  e.type = EntityType::SkeletonMage;
  e.position = player->position + glm::vec2(cos(a) * 1000, sin(a) * 1000);
  e.scale = {96, 96};
  e.uvScale = {0.25f, 0.25f};
  e.uvOffset = {0.5f, 0};             // Skeleton sprite
  e.color = {0.7f, 0.3f, 1.0f, 1.0f}; // Purple tint
  e.radius = 35;
  e.hp = 1000;
  e.maxHp = 1000;
  e.speed = 50;
  e.shootTimer = 0;
  e.summonTimer = 0;
  entities.push_back(e);
  std::cout << ">>> BOSS SPAWNED! <<<" << std::endl;
}

void Game::SpawnGem(glm::vec2 pos, int type) {
  Entity e;
  e.position = pos;
  e.scale = {64, 64};
  e.uvScale = {0.25f, 0.25f};
  e.radius = 15;
  if (type == 0) {
    e.type = EntityType::HealthGem;
    e.uvOffset = {0, 0.25f};
    e.color = {0.5f, 1, 0.5f, 1};
  } else {
    e.type = EntityType::PiercingGem;
    e.uvOffset = {0.25f, 0.25f};
    e.color = {1, 0.5f, 1, 1};
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
  b.uvOffset = {0.75f, 0};
  b.uvScale = {0.25f, 0.25f};
  b.radius = 10;
  b.color = {1, 1, 0, 1};
  b.lifeTime = 2;
  b.damage = bulletDamage;
  if (player->piercingTimer > 0) {
    b.penetration = 100;
    b.color = {1, 0.2f, 1, 1};
    b.scale = {48, 48};
  } else {
    b.penetration = bulletPenetration;
  }
  glm::vec2 dir = targetPos - player->position;
  b.velocity = (glm::length(dir) > 0.1f) ? glm::normalize(dir) * 600.0f
                                         : glm::vec2(600, 0);
  entities.push_back(b);
}

void Game::SpawnEnemyBullet(glm::vec2 from, glm::vec2 target) {
  Entity b;
  b.type = EntityType::EnemyBullet;
  b.position = from;
  b.scale = {48, 48}; // 2x bigger
  b.uvOffset = {0.75f, 0};
  b.uvScale = {0.25f, 0.25f};         // Crystal sprite
  b.color = {1.0f, 0.2f, 0.2f, 1.0f}; // Red
  b.radius = 16;                      // 2x radius
  b.lifeTime = 3;
  b.damage = 30; // 2x damage
  glm::vec2 dir = target - from;
  b.velocity = (glm::length(dir) > 0.1f) ? glm::normalize(dir) * 400.0f
                                         : glm::vec2(400, 0);
  entities.push_back(b);
}

int Game::FindNearestEnemy() {
  int n = -1;
  float md = 1e9;
  for (size_t i = 1; i < entities.size(); ++i) {
    auto t = entities[i].type;
    if (t != EntityType::Blob && t != EntityType::Skeleton &&
        t != EntityType::SkeletonMage)
      continue;
    glm::vec2 d = player->position - entities[i].position;
    float d2 = glm::dot(d, d);
    if (d2 < md && d2 < 640000) {
      md = d2;
      n = (int)i;
    }
  }
  return n;
}

std::vector<Upgrade> Game::GenerateUpgradeOptions() {
  std::vector<Upgrade> pool = {
      {UpgradeType::MaxHP, "+20 MAX HP", {0.2f, 1, 0.2f, 1}},
      {UpgradeType::Damage, "+5 DAMAGE", {1, 0.3f, 0.3f, 1}},
      {UpgradeType::FireRate, "FAST FIRE", {1, 1, 0.3f, 1}},
      {UpgradeType::Speed, "+50 SPEED", {0.3f, 0.7f, 1, 1}},
      {UpgradeType::Penetration, "+2 PIERCE", {0.8f, 0.3f, 1, 1}},
  };
  std::random_device rd;
  std::mt19937 rng(rd());
  std::shuffle(pool.begin(), pool.end(), rng);
  pool.resize(3);
  return pool;
}

void Game::TriggerLevelUp() {
  state = GameState::LevelUp;
  playerLevel++;
  xp = 0;
  xpToNextLevel = 10 + playerLevel * 15;
  currentUpgrades = GenerateUpgradeOptions();
  audio.PlaySFX(SFXType::LevelUp);
}

void Game::ApplyUpgrade(int choice) {
  if (choice < 0 || choice >= (int)currentUpgrades.size())
    return;
  switch (currentUpgrades[choice].type) {
  case UpgradeType::MaxHP:
    player->maxHp += 20;
    player->hp = player->maxHp;
    break;
  case UpgradeType::Damage:
    bulletDamage += 5;
    break;
  case UpgradeType::FireRate:
    fireCooldown = std::max(0.05f, fireCooldown - 0.05f);
    break;
  case UpgradeType::Speed:
    playerSpeed += 50;
    break;
  case UpgradeType::Penetration:
    bulletPenetration += 2;
    break;
  }
  state = GameState::Playing;
}

void Game::DrawText(std::vector<InstanceData> &data, float x, float y,
                    const std::string &text, glm::vec4 color, float charSize) {
  const float cellUV = 1.0f / 16.0f;
  for (size_t i = 0; i < text.size(); ++i) {
    int ch = (int)text[i] - 32;
    if (ch < 0 || ch >= 96)
      ch = 0;
    int col = ch % 16, row = ch / 16;
    InstanceData inst;
    inst.position = {x + i * charSize * 0.65f, y};
    inst.scale = {charSize, charSize};
    inst.uvOffset = {col * cellUV, row * cellUV};
    inst.uvScale = {cellUV, cellUV};
    inst.color = color;
    inst.useSolidColor = 0;
    data.push_back(inst);
  }
}

void Game::ProcessInput(float dt) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);
  if (state == GameState::GameOver) {
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
      ResetGame();
    return;
  }
  if (state == GameState::LevelUp) {
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
      ApplyUpgrade(0);
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
      ApplyUpgrade(1);
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
      ApplyUpgrade(2);
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
    player->position += glm::normalize(input) * playerSpeed * dt;
}

void Game::Update(float dt) {
  ProcessInput(dt);
  // Camera
  if (player) {
    float cx = player->position.x - width / 2.0f,
          cy = player->position.y - height / 2.0f;
    camUniforms.viewProj =
        glm::ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f) *
        glm::translate(glm::mat4(1), glm::vec3(-cx, -cy, 0));
    wgpuQueueWriteBuffer(queue, uniformBuffer, 0, &camUniforms,
                         sizeof(CameraUniforms));
  }
  if (state != GameState::Playing)
    return;

  gameTime += dt;
  spawnTimer += dt;
  fireTimer += dt;
  if (player->piercingTimer > 0)
    player->piercingTimer -= dt;

  // Recurring mini-boss spawn
  if (gameTime >= nextBossTime) {
    SpawnBoss();
    // Next boss sooner each time (min 20s interval)
    float interval = std::max(20.0f, 45.0f - gameTime * 0.1f);
    nextBossTime = gameTime + interval;
  }

  // Wave spawning
  float diff = 1 + (gameTime / 60) * 10;
  float interval = 0.5f / diff;
  if (interval < 0.05f)
    interval = 0.05f;
  if (spawnTimer >= interval && entities.size() < 15000) {
    spawnTimer = 0;
    SpawnEnemy();
  }

  // Player firing
  if (fireTimer >= fireCooldown) {
    int t = FindNearestEnemy();
    if (t != -1) {
      SpawnBullet(entities[t].position);
      fireTimer = 0;
      audio.PlaySFX(SFXType::Shoot);
    }
  }

  // === Entity Logic ===
  for (int i = (int)entities.size() - 1; i >= 1; --i) {
    if (i >= (int)entities.size())
      continue; // safety after removals
    Entity &e = entities[i];

    // --- Corpse lifeTime decay ---
    if (e.type == EntityType::SkeletonCorpse ||
        e.type == EntityType::BlobCorpse) {
      e.lifeTime -= dt;
      e.color.a = std::max(0.0f, e.lifeTime / 5.0f) * 0.6f; // Fade out
      if (e.lifeTime <= 0) {
        entities[i] = entities.back();
        entities.pop_back();
        continue;
      }
    }

    // --- Boss AI ---
    if (e.type == EntityType::SkeletonMage) {
      e.shootTimer += dt;
      e.summonTimer += dt;
      // Move toward player slowly
      glm::vec2 dir = player->position - e.position;
      float dist = glm::length(dir);
      if (dist > 200)
        e.position += glm::normalize(dir) * e.speed * dt;
      // Shoot at player every 2s
      if (e.shootTimer >= 2.0f) {
        e.shootTimer = 0;
        SpawnEnemyBullet(e.position, player->position);
        // Shoot 3 bullets in spread
        glm::vec2 d2 = player->position - e.position;
        if (glm::length(d2) > 0.1f) {
          float angle = atan2(d2.y, d2.x);
          SpawnEnemyBullet(e.position,
                           e.position +
                               glm::vec2(cos(angle + 0.3f), sin(angle + 0.3f)) *
                                   500.0f);
          SpawnEnemyBullet(e.position,
                           e.position +
                               glm::vec2(cos(angle - 0.3f), sin(angle - 0.3f)) *
                                   500.0f);
        }
      }
      // Necromancy: resurrect nearby corpses every 5s
      if (e.summonTimer >= 5.0f) {
        e.summonTimer = 0;
        bool resurrected = false;
        for (size_t c = 1; c < entities.size(); ++c) {
          Entity &corpse = entities[c];
          if (corpse.type != EntityType::SkeletonCorpse)
            continue;
          if (glm::distance(e.position, corpse.position) > 500)
            continue; // Too far
          resurrected = true;
          corpse.type = EntityType::Skeleton;
          corpse.hp = 30;
          corpse.maxHp = 30;
          corpse.speed = 150;
          corpse.scale = {64, 64};
          corpse.uvOffset = {0.5f, 0};
          corpse.uvScale = {0.25f, 0.25f};
          corpse.radius = 25;
          corpse.color = {1, 0.7f, 0.7f, 1};
        }
        if (resurrected)
          audio.PlaySFX(SFXType::Death); // Necromancy sound
      }
    }

    // --- Enemy Bullets ---
    if (e.type == EntityType::EnemyBullet) {
      e.position += e.velocity * dt;
      e.lifeTime -= dt;
      if (e.lifeTime <= 0) {
        entities[i] = entities.back();
        entities.pop_back();
        continue;
      }
      if (glm::distance(e.position, player->position) <
          (player->radius + e.radius)) {
        player->hp -= e.damage;
        entities[i] = entities.back();
        entities.pop_back();
        if (player->hp <= 0) {
          state = GameState::GameOver;
          audio.PlaySFX(SFXType::Death);
        }
        continue;
      }
    }

    // --- Player Bullets ---
    if (e.type == EntityType::Bullet) {
      e.position += e.velocity * dt;
      e.lifeTime -= dt;
      if (e.lifeTime <= 0) {
        entities[i] = entities.back();
        entities.pop_back();
        continue;
      }
      for (int j = 1; j < (int)entities.size(); ++j) {
        Entity &tgt = entities[j];
        bool isEnemy =
            (tgt.type == EntityType::Blob || tgt.type == EntityType::Skeleton ||
             tgt.type == EntityType::SkeletonMage);
        if (!isEnemy)
          continue;
        if (glm::distance(e.position, tgt.position) < (tgt.radius + e.radius)) {
          tgt.hp -= e.damage;
          e.penetration--;
          if (tgt.hp <= 0) {
            audio.PlaySFX(SFXType::Hit);
            glm::vec2 dpos = tgt.position;
            if (tgt.type == EntityType::SkeletonMage) {
              // Boss dies — big reward
              score += 50;
              for (int g = 0; g < 5; ++g)
                SpawnGem(dpos + glm::vec2((g - 2) * 40.0f, 0), g % 2);
              entities[j] = entities.back();
              entities.pop_back();
            } else {
              static std::random_device rd;
              static std::mt19937 rng(rd());
              int val = std::uniform_int_distribution<int>(0, 100)(rng);
              if (val > 98) {
                SpawnGem(dpos, 1);
                entities[j] = entities.back();
                entities.pop_back();
              } else if (val > 95) {
                SpawnGem(dpos, 0);
                entities[j] = entities.back();
                entities.pop_back();
              } else {
                // Remember original type before converting
                EntityType origType = tgt.type;
                // Drop crystal for XP
                tgt.type = EntityType::Crystal;
                tgt.color = {0.5f, 1, 1, 1};
                tgt.uvOffset = {0.75f, 0};
                tgt.radius = 15;
                // Also spawn background corpse (bones/slime)
                Entity corpse;
                corpse.position = dpos;
                corpse.scale = {48, 48};
                corpse.uvScale = {0.25f, 0.25f};
                corpse.radius = 0;      // No collision
                corpse.lifeTime = 5.0f; // Disappears after 5s
                if (origType == EntityType::Skeleton) {
                  corpse.type = EntityType::SkeletonCorpse;
                  corpse.uvOffset = {0.5f, 0.25f};
                  corpse.color = {0.8f, 0.8f, 0.8f, 0.6f};
                } else {
                  corpse.type = EntityType::BlobCorpse;
                  corpse.uvOffset = {0.75f, 0.25f};
                  corpse.color = {0.6f, 0.9f, 0.6f, 0.6f};
                }
                entities.push_back(corpse);
              }
            }
          } else {
            // Only pushback for non-piercing bullets (piercing passes through)
            if (e.penetration <= 0)
              tgt.position += glm::normalize(e.velocity) * 10.0f;
          }
          if (e.penetration <= 0)
            break;
        }
      }
      if (e.penetration <= 0) {
        entities[i] = entities.back();
        entities.pop_back();
        continue;
      }
    }

    // --- Collections ---
    if (i >= (int)entities.size())
      continue;
    Entity &ec = entities[i];
    if (ec.type == EntityType::Crystal) {
      if (glm::distance(player->position, ec.position) <
          (player->radius + ec.radius + 20)) {
        xp++;
        score++;
        audio.PlaySFX(SFXType::Collect);
        entities[i] = entities.back();
        entities.pop_back();
        if (xp >= xpToNextLevel)
          TriggerLevelUp();
      }
    } else if (ec.type == EntityType::HealthGem) {
      if (glm::distance(player->position, ec.position) <
          (player->radius + ec.radius + 20)) {
        player->hp = std::min(player->hp + 20, player->maxHp);
        audio.PlaySFX(SFXType::Collect);
        entities[i] = entities.back();
        entities.pop_back();
      }
    } else if (ec.type == EntityType::PiercingGem) {
      if (glm::distance(player->position, ec.position) <
          (player->radius + ec.radius + 20)) {
        player->piercingTimer = 10;
        audio.PlaySFX(SFXType::Collect);
        entities[i] = entities.back();
        entities.pop_back();
      }
    }

    // --- Contact Damage ---
    if (i >= (int)entities.size())
      continue;
    Entity &ed = entities[i];
    if (ed.type == EntityType::Blob || ed.type == EntityType::Skeleton ||
        ed.type == EntityType::SkeletonMage) {
      if (glm::distance(player->position, ed.position) <
          (player->radius + ed.radius - 5)) {
        float dmg = (ed.type == EntityType::SkeletonMage) ? 40.0f : 20.0f;
        player->hp -= dmg * dt;
        if (player->hp <= 0) {
          state = GameState::GameOver;
          audio.PlaySFX(SFXType::Death);
        }
      }
    }
  }
  player = &entities[0];

  // === Spatial Grid: Movement + Separation ===
  const float SZ = 100;
  const int W = 40, H = 40;
  const float OFF = 2000;
  static std::vector<int> grid[W][H];
  for (int x = 0; x < W; ++x)
    for (int y = 0; y < H; ++y)
      grid[x][y].clear();
  for (size_t i = 1; i < entities.size(); ++i) {
    Entity &e = entities[i];
    bool isEnemy =
        (e.type == EntityType::Blob || e.type == EntityType::Skeleton ||
         e.type == EntityType::SkeletonMage);
    if (!isEnemy)
      continue;
    // Move toward player using per-entity speed
    glm::vec2 dir = player->position - e.position;
    float dist = glm::length(dir);
    if (dist > 30 &&
        e.type != EntityType::SkeletonMage) // Boss handles its own movement
      e.position += glm::normalize(dir) * e.speed * dt;
    int gx = (int)((e.position.x + OFF) / SZ),
        gy = (int)((e.position.y + OFF) / SZ);
    if (gx >= 0 && gx < W && gy >= 0 && gy < H)
      grid[gx][gy].push_back((int)i);
  }
  for (int iter = 0; iter < 2; ++iter) {
    for (size_t i = 1; i < entities.size(); ++i) {
      Entity &e = entities[i];
      bool isEnemy =
          (e.type == EntityType::Blob || e.type == EntityType::Skeleton ||
           e.type == EntityType::SkeletonMage);
      if (!isEnemy)
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
            glm::vec2 d = e.position - o.position;
            float d2 = glm::dot(d, d);
            float rSum = e.radius + o.radius;
            if (d2 < rSum * rSum && d2 > 0.001f) {
              float dist2 = std::sqrt(d2);
              e.position += (d / dist2) * (rSum - dist2) * 0.8f;
            }
          }
        }
    }
  }
}

void Game::Render() {
  WGPUSurfaceTexture st;
  wgpuSurfaceGetCurrentTexture(surface, &st);
  if (!st.texture)
    return;
  WGPUTextureView tv = wgpuTextureCreateView(st.texture, nullptr);
  WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
  WGPURenderPassColorAttachment ca = {};
  ca.view = tv;
  ca.loadOp = WGPULoadOp_Clear;
  ca.storeOp = WGPUStoreOp_Store;
  ca.clearValue = {0.05, 0.05, 0.1, 1};
  WGPURenderPassDescriptor rpd = {};
  rpd.colorAttachmentCount = 1;
  rpd.colorAttachments = &ca;

  static std::vector<InstanceData> spriteData, textData;
  spriteData.clear();
  textData.clear();
  spriteData.reserve(entities.size() + 30);

  for (const auto &e : entities)
    spriteData.push_back(
        {e.position, e.scale, e.uvOffset, e.uvScale, e.color, 0});

  // Player HP Bar
  if (player && state != GameState::GameOver) {
    spriteData.push_back({player->position + glm::vec2(0, -50),
                          {80, 10},
                          {0, 0},
                          {0, 0},
                          {0.3f, 0, 0, 1},
                          1});
    float pct = std::max(0.0f, player->hp / player->maxHp);
    spriteData.push_back({player->position + glm::vec2(-40 + (40 * pct), -50),
                          {80 * pct, 10},
                          {0, 0},
                          {0, 0},
                          {0, 1, 0, 1},
                          1});
    // XP Bar
    float xpPct = (float)xp / (float)xpToNextLevel;
    spriteData.push_back({player->position + glm::vec2(0, -38),
                          {80, 6},
                          {0, 0},
                          {0, 0},
                          {0, 0, 0.3f, 1},
                          1});
    spriteData.push_back({player->position + glm::vec2(-40 + (40 * xpPct), -38),
                          {80 * xpPct, 6},
                          {0, 0},
                          {0, 0},
                          {0.3f, 0.5f, 1, 1},
                          1});
  }

  // Boss HP Bar (big, red, visible)
  for (const auto &e : entities) {
    if (e.type == EntityType::SkeletonMage) {
      float bpct = std::max(0.0f, e.hp / e.maxHp);
      spriteData.push_back({e.position + glm::vec2(0, -60),
                            {100, 12},
                            {0, 0},
                            {0, 0},
                            {0.4f, 0, 0, 1},
                            1});
      spriteData.push_back({e.position + glm::vec2(-50 + (50 * bpct), -60),
                            {100 * bpct, 12},
                            {0, 0},
                            {0, 0},
                            {0.8f, 0.2f, 1, 1},
                            1});
    }
  }

  // Overlays
  if (state == GameState::GameOver)
    spriteData.push_back({player->position,
                          {(float)width, (float)height},
                          {0, 0},
                          {0, 0},
                          {0.8f, 0, 0, 0.5f},
                          1});
  if (state == GameState::LevelUp) {
    spriteData.push_back({player->position,
                          {(float)width, (float)height},
                          {0, 0},
                          {0, 0},
                          {0, 0, 0.2f, 0.7f},
                          1});
    for (int i = 0; i < 3; ++i) {
      float bx = player->position.x - 200 + i * 200, by = player->position.y;
      spriteData.push_back(
          {{bx, by}, {150, 80}, {0, 0}, {0, 0}, {0.1f, 0.1f, 0.15f, 0.9f}, 1});
      spriteData.push_back({{bx, by - 30},
                            {140, 4},
                            {0, 0},
                            {0, 0},
                            currentUpgrades[i].color,
                            1});
    }
  }

  // === Text ===
  glm::vec2 cam = player ? player->position : glm::vec2(0);
  if (state == GameState::Playing) {
    DrawText(textData, cam.x - width / 2.0f + 10, cam.y - height / 2.0f + 10,
             "LVL:" + std::to_string(playerLevel), {1, 1, 1, 1}, 20);
    DrawText(textData, cam.x - width / 2.0f + 10, cam.y - height / 2.0f + 35,
             "SCORE:" + std::to_string(score), {0.8f, 0.8f, 0.3f, 1}, 20);
    DrawText(textData, cam.x - width / 2.0f + 10, cam.y - height / 2.0f + 60,
             "HP:" + std::to_string((int)player->hp) + "/" +
                 std::to_string((int)player->maxHp),
             {0.3f, 1, 0.3f, 1}, 18);
    if (player->piercingTimer > 0)
      DrawText(textData, cam.x - width / 2.0f + 10, cam.y - height / 2.0f + 85,
               "PIERCING! " + std::to_string((int)player->piercingTimer) + "s",
               {1, 0.3f, 1, 1}, 18);
    {
      // Check if boss is alive
      bool bossAlive = false;
      for (const auto &e : entities)
        if (e.type == EntityType::SkeletonMage)
          bossAlive = true;
      if (bossAlive)
        DrawText(textData, cam.x - 60, cam.y - height / 2.0f + 10, "!! BOSS !!",
                 {1, 0.2f, 0.2f, 1}, 24);
    }
  }
  if (state == GameState::GameOver) {
    DrawText(textData, cam.x - 120, cam.y - 40, "GAME OVER", {1, 0.2f, 0.2f, 1},
             40);
    DrawText(textData, cam.x - 100, cam.y + 20,
             "SCORE: " + std::to_string(score), {1, 1, 1, 1}, 24);
    DrawText(textData, cam.x - 80, cam.y + 60, "PRESS R", {1, 1, 0.5f, 1}, 24);
  }
  if (state == GameState::LevelUp) {
    DrawText(textData, cam.x - 100, cam.y - 100, "LEVEL UP!", {1, 1, 0.2f, 1},
             36);
    for (int i = 0; i < 3; ++i) {
      float bx = cam.x - 200 + i * 200;
      DrawText(textData, bx - 60, cam.y - 10,
               "[" + std::to_string(i + 1) + "] " + currentUpgrades[i].name,
               currentUpgrades[i].color, 14);
    }
  }

  // === Render ===
  WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rpd);
  wgpuRenderPassEncoderSetPipeline(pass, pipeline);
  wgpuRenderPassEncoderSetBindGroup(pass, 0, camBindGroup, 0, nullptr);
  wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertexBuffer, 0, 4 * 16);
  wgpuRenderPassEncoderSetIndexBuffer(pass, indexBuffer, WGPUIndexFormat_Uint16,
                                      0, 12);
  if (!spriteData.empty()) {
    wgpuQueueWriteBuffer(queue, instanceBuffer, 0, spriteData.data(),
                         spriteData.size() * sizeof(InstanceData));
    wgpuRenderPassEncoderSetBindGroup(pass, 1, texBindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(
        pass, 1, instanceBuffer, 0, spriteData.size() * sizeof(InstanceData));
    wgpuRenderPassEncoderDrawIndexed(pass, 6, (uint32_t)spriteData.size(), 0, 0,
                                     0);
  }
  if (!textData.empty()) {
    size_t off = spriteData.size() * sizeof(InstanceData);
    wgpuQueueWriteBuffer(queue, instanceBuffer, off, textData.data(),
                         textData.size() * sizeof(InstanceData));
    wgpuRenderPassEncoderSetBindGroup(pass, 1, fontTexBindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(
        pass, 1, instanceBuffer, off, textData.size() * sizeof(InstanceData));
    wgpuRenderPassEncoderDrawIndexed(pass, 6, (uint32_t)textData.size(), 0, 0,
                                     0);
  }
  wgpuRenderPassEncoderEnd(pass);
  WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
  wgpuQueueSubmit(queue, 1, &cmd);
  wgpuSurfacePresent(surface);
  wgpuCommandBufferRelease(cmd);
  wgpuCommandEncoderRelease(enc);
  wgpuRenderPassEncoderRelease(pass);
  wgpuTextureViewRelease(tv);
  wgpuTextureRelease(st.texture);
}

void Game::Run() {
  float lastTime = (float)glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    float cur = (float)glfwGetTime();
    float dt = cur - lastTime;
    lastTime = cur;
    if (dt > 0.1f)
      dt = 0.1f;
    glfwPollEvents();
    Update(dt);
    Render();
    static float timer = 0;
    static int frames = 0;
    timer += dt;
    frames++;
    if (timer >= 1) {
      std::string s = (state == GameState::GameOver)
                          ? " [GAME OVER]"
                          : (state == GameState::LevelUp ? " [LEVEL UP!]" : "");
      glfwSetWindowTitle(window, ("WarpEngine | FPS:" + std::to_string(frames) +
                                  " | Lvl:" + std::to_string(playerLevel) +
                                  " | Score:" + std::to_string(score) + s)
                                     .c_str());
      timer = 0;
      frames = 0;
    }
  }
}

void Game::Cleanup() {
  audio.Cleanup();
  if (window)
    glfwDestroyWindow(window);
  glfwTerminate();
}

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
