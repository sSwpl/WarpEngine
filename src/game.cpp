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
  player->colliderSize = {30.0f, 40.0f}; // Slimmer width for player
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
    e.colliderSize = {30, 30};
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
  // Wave system
  currentWave = 0;
  waveTimer = 0.0f;
  waveDuration = 35.0f;
  waveBossSpawned = false;
  waveBossAlive = false;
  endlessMode = false;
  endlessTimer = 0.0f;
  // Difficulty Director
  difficultyRating = 0.3f;
  targetDifficulty = 0.3f;
  perf = PerformanceMetrics{};
  diffOut = DifficultyOutput{};
  // Dodge roll
  dodgeTimer = dodgeCooldown;
  dodging = false;
  dodgeTimeLeft = 0.0f;
  lastMoveDir = {1, 0};
  state = GameState::WeaponSelect;
}

void Game::SpawnEnemy(int enemyType) {
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_real_distribution<float> da(0, 6.28f), dr(900, 1300);
  float a = da(rng), r = dr(rng);
  Entity e;
  e.position = player->position + glm::vec2(cos(a) * r, sin(a) * r);
  e.uvScale = {0.25f, 0.25f};

  // Use separate multipliers from difficulty director
  float hpM = diffOut.enemyHpMult;
  float dmgM = diffOut.enemyDamageMult;
  float spdM = diffOut.enemySpeedMult;

  switch (enemyType) {
  case 0: // Small Skeletons
    e.type = EntityType::Skeleton;
    e.scale = {64, 64};
    e.uvOffset = {0.5f, 0};
    e.color = {1, 0.9f, 0.9f, 1};
    e.hp = 30 * hpM;
    e.maxHp = e.hp;
    e.speed = 150 * spdM;
    e.radius = 18;
    e.colliderSize = {36, 36};
    e.contactDamage = 15.0f * dmgM;
    break;
  case 1: // Small Slimes
    e.type = EntityType::Blob;
    e.scale = {64, 64};
    e.uvOffset = {0.25f, 0};
    e.color = {0.8f, 1, 0.8f, 1};
    e.hp = 60 * hpM;
    e.maxHp = e.hp;
    e.speed = 110 * spdM;
    e.radius = 18;
    e.colliderSize = {36, 36};
    e.contactDamage = 25.0f * dmgM;
    break;
  case 2: // Big Skeletons
    e.type = EntityType::Skeleton;
    e.scale = {96, 96};
    e.uvOffset = {0.5f, 0};
    e.color = {1, 0.7f, 0.7f, 1};
    e.hp = 80 * hpM;
    e.maxHp = e.hp;
    e.speed = 130 * spdM;
    e.radius = 28;
    e.colliderSize = {48, 48};
    e.contactDamage = 30.0f * dmgM;
    break;
  case 3: // Big Slimes
    e.type = EntityType::Blob;
    e.scale = {96, 96};
    e.uvOffset = {0.25f, 0};
    e.color = {0.5f, 1.0f, 0.5f, 1};
    e.hp = 100 * hpM;
    e.maxHp = e.hp;
    e.speed = 100 * spdM;
    e.radius = 28;
    e.colliderSize = {48, 48};
    e.contactDamage = 35.0f * dmgM;
    break;
  default:
    return SpawnEnemy(rng() % 4);
  }
  entities.push_back(e);
}

void Game::SpawnBoss(int bossType) {
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_real_distribution<float> da(0, 6.28f);
  float a = da(rng);
  float bhp = diffOut.bossHpMult;
  float bdmg = diffOut.bossDamageMult;
  Entity e;
  e.type = EntityType::SkeletonMage;
  e.position = player->position + glm::vec2(cos(a) * 1000, sin(a) * 1000);
  e.uvScale = {0.25f, 0.25f};
  e.shootTimer = 0;
  e.summonTimer = 0;

  switch (bossType) {
  case 0: // Skeleton Mage
    e.scale = {96, 96};
    e.uvOffset = {0.5f, 0};
    e.color = {0.7f, 0.3f, 1.0f, 1.0f};
    e.radius = 35;
    e.colliderSize = {60, 60};
    e.hp = 800 * bhp;
    e.maxHp = e.hp;
    e.speed = 50;
    e.contactDamage = 40.0f * bdmg;
    break;
  case 1: // Slime Boss
    e.scale = {128, 128};
    e.uvOffset = {0.25f, 0};
    e.color = {0.4f, 1.0f, 0.4f, 1.0f};
    e.radius = 50;
    e.colliderSize = {80, 80};
    e.hp = 1200 * bhp;
    e.maxHp = e.hp;
    e.speed = 40;
    e.contactDamage = 50.0f * bdmg;
    break;
  case 2: // Big Skeleton Mage
    e.scale = {128, 128};
    e.uvOffset = {0.5f, 0};
    e.color = {0.9f, 0.2f, 0.8f, 1.0f};
    e.radius = 50;
    e.colliderSize = {80, 80};
    e.hp = 1800 * bhp;
    e.maxHp = e.hp;
    e.speed = 45;
    e.contactDamage = 55.0f * bdmg;
    break;
  case 3: // Big Slime Boss
    e.scale = {160, 160};
    e.uvOffset = {0.25f, 0};
    e.color = {0.2f, 0.9f, 0.2f, 1.0f};
    e.radius = 60;
    e.colliderSize = {100, 100};
    e.hp = 2500 * bhp;
    e.maxHp = e.hp;
    e.speed = 35;
    e.contactDamage = 60.0f * bdmg;
    break;
  }
  entities.push_back(e);
  std::cout << ">>> BOSS (type " << bossType
            << ") SPAWNED! Diff=" << difficultyRating
            << " Power=" << CalculatePlayerPower() << " <<<" << std::endl;
}

void Game::SpawnGem(glm::vec2 pos, int type) {
  Entity e;
  e.position = pos;
  e.scale = {64, 64};
  e.uvScale = {0.25f, 0.25f};
  e.radius = 15;
  e.colliderSize = {30, 30};
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

// Central enemy death handler — drops crystals, corpses, gems, score
void Game::HandleEnemyDeath(int idx) {
  Entity &tgt = entities[idx];
  glm::vec2 dpos = tgt.position;
  EntityType origType = tgt.type;

  perf.windowKills++;
  perf.totalKills++;
  audio.PlaySFX(SFXType::Hit);

  if (origType == EntityType::SkeletonMage) {
    score += 50;
    for (int g = 0; g < 5; ++g)
      SpawnGem(dpos + glm::vec2((g - 2) * 40.0f, 0), g % 2);
    entities[idx] = entities.back();
    entities.pop_back();
  } else {
    score += 1;
    static std::random_device rd;
    static std::mt19937 rng(rd());
    int val = std::uniform_int_distribution<int>(0, 100)(rng);
    if (val > 98) {
      SpawnGem(dpos, 1);
      entities[idx] = entities.back();
      entities.pop_back();
    } else if (val > 95) {
      SpawnGem(dpos, 0);
      entities[idx] = entities.back();
      entities.pop_back();
    } else {
      // Drop crystal for XP
      tgt.type = EntityType::Crystal;
      tgt.scale = {64, 64};
      tgt.colliderSize = {30, 30};
      tgt.color = {0.5f, 1, 1, 1};
      tgt.uvOffset = {0.75f, 0};
      tgt.radius = 15;
      // Also spawn background corpse
      Entity corpse;
      corpse.position = dpos;
      corpse.scale = {48, 48};
      corpse.uvScale = {0.25f, 0.25f};
      corpse.radius = 0;
      corpse.lifeTime = 5.0f;
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
}

void Game::SpawnBullet(glm::vec2 targetPos) {
  if (!player)
    return;

  if (currentWeapon == WeaponType::Sword) {
    // --- SWORD: melee arc swing ---
    bool fullCircle = (player->piercingTimer > 0); // Power-up: 360° swing
    Entity sw;
    sw.type = EntityType::SwordSwing;
    glm::vec2 sdir = targetPos - player->position;
    glm::vec2 snorm =
        (glm::length(sdir) > 0.1f) ? glm::normalize(sdir) : glm::vec2(1, 0);
    if (fullCircle) {
      sw.position = player->position; // Centered for 360
      sw.scale = {200, 200};
      sw.radius = 120;
      sw.colliderSize = {200, 200};
      sw.color = {1.0f, 0.4f, 1.0f, 0.7f}; // Purple glow
      sw.uvOffset = {0.75f, 0.5f};         // Slash sprite from atlas
      sw.uvScale = {0.25f, 0.25f};
    } else {
      sw.position = player->position + snorm * 60.0f;
      sw.scale = {120, 60}; // Thin slash shape
      sw.radius = 120;
      sw.colliderSize = {120, 120};
      sw.color = {0.8f, 0.9f, 1.0f, 0.7f};
      sw.uvOffset = {0.75f, 0.5f}; // Slash sprite from atlas
      sw.uvScale = {0.25f, 0.25f};
    }
    sw.lifeTime = 0.15f;
    sw.damage = bulletDamage * 3;
    sw.penetration = 999;
    sw.velocity = snorm;
    // For full circle, set angle to allow 360
    sw.contactDamage = fullCircle ? 1.0f : 0.0f; // Reuse as flag
    entities.push_back(sw);
    return;
  }

  if (currentWeapon == WeaponType::Bazooka) {
    // --- BAZOOKA: big slow explosive projectile ---
    Entity b;
    b.type = EntityType::Bullet;
    b.position = player->position;
    b.scale = {64, 64};
    b.uvOffset = {0.75f, 0};
    b.uvScale = {0.25f, 0.25f};
    b.radius = 20;
    b.colliderSize = {40, 40};
    b.color = {1, 0.5f, 0.1f, 1}; // Orange
    b.lifeTime = 3;
    b.damage = bulletDamage * 4; // 4x damage
    b.penetration = 1;           // Explodes on first hit
    glm::vec2 dir = targetPos - player->position;
    b.velocity = (glm::length(dir) > 0.1f) ? glm::normalize(dir) * 500.0f
                                           : glm::vec2(500, 0);
    entities.push_back(b);
    return;
  }

  // --- MACHINE GUN: default ---
  Entity b;
  b.type = EntityType::Bullet;
  b.position = player->position;
  b.scale = {32, 32};
  b.uvOffset = {0.75f, 0};
  b.uvScale = {0.25f, 0.25f};
  b.radius = 10;
  b.colliderSize = {20, 20};
  b.color = {1, 1, 0, 1};
  b.lifeTime = 2;
  b.damage = bulletDamage;
  if (player->piercingTimer > 0) {
    b.penetration = 5;
    b.color = {1, 0.2f, 1, 1};
    b.scale = {40, 40};
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
  b.colliderSize = {32, 32};
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
      {UpgradeType::DashCooldown, "FASTER DASH", {0.3f, 1, 1, 1}},
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
    bulletPenetration += 1;
    break;
  case UpgradeType::DashCooldown:
    dodgeCooldown = std::max(0.5f, dodgeCooldown - 0.3f);
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
  if (state == GameState::WeaponSelect) {
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
      currentWeapon = WeaponType::MachineGun;
      fireCooldown = 0.2f;
      state = GameState::Playing;
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
      currentWeapon = WeaponType::Sword;
      fireCooldown = 0.4f;
      state = GameState::Playing;
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
      currentWeapon = WeaponType::Bazooka;
      fireCooldown = 1.5f;
      state = GameState::Playing;
    }
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
  if (glm::length(input) > 0) {
    lastMoveDir = glm::normalize(input);
    if (!dodging)
      player->position += lastMoveDir * playerSpeed * dt;
  }
  // Dodge roll on spacebar
  static bool spaceWasPressed = false;
  bool spaceNow = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
  if (spaceNow && !spaceWasPressed && !dodging && dodgeTimer >= dodgeCooldown) {
    dodging = true;
    dodgeTimeLeft = dodgeDuration;
    dodgeDir = lastMoveDir;
    dodgeTimer = 0;
  }
  spaceWasPressed = spaceNow;
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
  dodgeTimer += dt;

  // Dodge roll movement
  if (dodging) {
    player->position += dodgeDir * dodgeSpeed * dt;
    dodgeTimeLeft -= dt;
    if (dodgeTimeLeft <= 0) {
      dodging = false;
    }
  }

  // === Difficulty Director ===
  UpdatePerformanceMetrics(dt);
  UpdateDifficultyDirector(dt);
  diffOut = CalculateDifficultyOutput();

  // === Wave System (4 waves + endless) ===
  waveTimer += dt;

  if (endlessMode) {
    endlessTimer += dt;
    // ENDLESS MODE: spawn all types, escalating
    float interval = 0.4f / diffOut.spawnRateMult;
    if (interval < 0.03f)
      interval = 0.03f;
    if (spawnTimer >= interval && entities.size() < 15000) {
      spawnTimer = 0;
      static std::mt19937 erng(std::random_device{}());
      int maxType = std::min(3, (int)(endlessTimer / 20.0f));
      SpawnEnemy(erng() % (maxType + 1));
    }
  } else if (!waveBossAlive) {
    int enemyType = currentWave;
    float interval = 0.5f / diffOut.spawnRateMult;
    if (interval < 0.06f)
      interval = 0.06f;
    if (spawnTimer >= interval && entities.size() < 15000) {
      spawnTimer = 0;
      SpawnEnemy(enemyType);
    }
    // Transition to boss after waveDuration
    if (waveTimer >= waveDuration && !waveBossSpawned) {
      waveBossSpawned = true;
      waveBossAlive = true;
      SpawnBoss(currentWave); // Boss type matches wave
      std::cout << ">> Wave " << (currentWave + 1) << " BOSS PHASE!"
                << std::endl;
    }
  } else {
    // Boss is alive — still trickle enemies
    float bossInterval = 0.7f;
    if (spawnTimer >= bossInterval && entities.size() < 15000) {
      spawnTimer = 0;
      SpawnEnemy(currentWave);
    }
    // Check if boss is dead
    bool found = false;
    for (const auto &ent : entities) {
      if (ent.type == EntityType::SkeletonMage) {
        found = true;
        break;
      }
    }
    if (!found) {
      waveBossAlive = false;
      waveBossSpawned = false;
      currentWave++;
      waveTimer = 0;
      if (currentWave >= 4) {
        // All 4 waves complete — enter ENDLESS MODE
        endlessMode = true;
        endlessTimer = 0;
        std::cout << ">>> ENDLESS MODE ACTIVATED! <<<" << std::endl;
      } else {
        std::cout << ">> Wave " << (currentWave + 1) << " started!"
                  << std::endl;
      }
    }
  }

  // Player firing
  if (fireTimer >= fireCooldown) {
    int t = FindNearestEnemy();
    if (t != -1) {
      SpawnBullet(entities[t].position);
      // Bazooka power-up: fire 2 extra rockets in spread
      if (currentWeapon == WeaponType::Bazooka && player->piercingTimer > 0) {
        glm::vec2 d = entities[t].position - player->position;
        if (glm::length(d) > 0.1f) {
          float a = atan2(d.y, d.x);
          SpawnBullet(player->position +
                      glm::vec2(cos(a + 0.25f), sin(a + 0.25f)) * 400.0f);
          SpawnBullet(player->position +
                      glm::vec2(cos(a - 0.25f), sin(a - 0.25f)) * 400.0f);
        }
      }
      fireTimer = 0;
      audio.PlaySFX(SFXType::Shoot);
    }
  }

  // === Entity Logic ===
  for (int i = (int)entities.size() - 1; i >= 1; --i) {
    if (i >= (int)entities.size())
      continue; // safety after removals
    Entity &e = entities[i];

    // --- Animation Update ---
    if (e.anim.frameCount > 1) {
      e.anim.timer += dt;
      if (e.anim.timer >= e.anim.frameDuration) {
        e.anim.timer = 0.0f;
        e.anim.currentFrame = (e.anim.currentFrame + 1) % e.anim.frameCount;
        // Calculate new UVs based on frame
        // Assuming horizontal strip animation for now
        int col = e.anim.startFrameX + e.anim.currentFrame;
        int row = e.anim.startFrameY;
        e.uvOffset.x = col * e.uvScale.x;
        e.uvOffset.y = row * e.uvScale.y;
      }
    }

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
      // Necromancy: resurrect nearby corpses every 5s (skeleton mage only)
      if (e.summonTimer >= 5.0f && e.uvOffset.x > 0.4f) {
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
          corpse.radius = 18;
          corpse.color = {1, 0.7f, 0.7f, 1};
        }
        if (resurrected)
          audio.PlaySFX(SFXType::Death); // Necromancy sound
      }
      // Big Slime Boss AOE: fire 8 bullets in a circle every 4s
      if (e.summonTimer >= 4.0f && e.uvOffset.x < 0.4f) {
        e.summonTimer = 0;
        for (int b = 0; b < 8; ++b) {
          float angle = b * (6.2832f / 8.0f);
          SpawnEnemyBullet(e.position,
                           e.position +
                               glm::vec2(cos(angle), sin(angle)) * 500.0f);
        }
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
      // Check AABB vs Player
      if (CheckCollisionAABB(e, *player)) {
        float d = e.damage;
        player->hp -= d;
        perf.windowDmgTaken += d;
        perf.totalDmgTaken += d;
        perf.timeSinceLastHit = 0;
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

        // Check AABB vs Enemy
        if (CheckCollisionAABB(e, tgt)) {
          float dealt = e.damage;
          tgt.hp -= dealt;
          perf.windowDmgDealt += dealt;
          perf.totalDmgDealt += dealt;
          e.penetration--;
          if (tgt.hp <= 0) {
            HandleEnemyDeath(j);
            if (j <= i)
              --i;
            --j;
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
        // Bazooka explosion on bullet death
        if (currentWeapon == WeaponType::Bazooka) {
          Entity exp;
          exp.type = EntityType::Explosion;
          exp.position = e.position;
          exp.scale = {240, 240};
          exp.uvOffset = {0, 0};
          exp.uvScale = {0, 0};
          exp.radius = 120;
          exp.colliderSize = {240, 240};
          exp.lifeTime = 0.3f;
          exp.damage = e.damage; // Same damage as rocket
          exp.color = {1, 0.6f, 0.1f, 0.8f};
          exp.penetration = 999; // Flag: needs to deal damage
          entities.push_back(exp);
          audio.PlaySFX(SFXType::Hit);
        }
        entities[i] = entities.back();
        entities.pop_back();
        continue;
      }
    }

    // --- Sword Swing ---
    if (e.type == EntityType::SwordSwing) {
      e.lifeTime -= dt;
      e.color.a = e.lifeTime / 0.15f * 0.7f; // Fade out
      if (e.lifeTime <= 0) {
        entities[i] = entities.back();
        entities.pop_back();
        continue;
      }
      // Damage all enemies in range
      if (e.penetration > 0) {
        bool fullCircle = (e.contactDamage > 0.5f); // Flag for 360 swing
        float swingAngle = atan2(e.velocity.y, e.velocity.x);
        for (int j = 1; j < (int)entities.size(); ++j) {
          Entity &tgt = entities[j];
          bool isEnemy = (tgt.type == EntityType::Blob ||
                          tgt.type == EntityType::Skeleton ||
                          tgt.type == EntityType::SkeletonMage);
          if (!isEnemy)
            continue;
          float dist = glm::distance(e.position, tgt.position);
          if (dist > e.radius)
            continue;
          glm::vec2 toE = tgt.position - e.position;
          if (!fullCircle) {
            // Check angle (120 degree arc)
            float eAngle = atan2(toE.y, toE.x);
            float diff = eAngle - swingAngle;
            while (diff > 3.14159f)
              diff -= 6.28318f;
            while (diff < -3.14159f)
              diff += 6.28318f;
            if (std::abs(diff) > 1.047f)
              continue;
          }
          // HIT!
          float dealt = e.damage;
          tgt.hp -= dealt;
          perf.windowDmgDealt += dealt;
          perf.totalDmgDealt += dealt;
          // Knockback
          if (dist > 0.1f)
            tgt.position += glm::normalize(toE) * 30.0f;
          if (tgt.hp <= 0) {
            HandleEnemyDeath(j);
            if (j <= i)
              --i;
            --j;
          }
        }
        e.penetration = 0; // Prevent multi-hit per swing
      }
    }

    // --- Explosion AoE ---
    if (e.type == EntityType::Explosion) {
      e.lifeTime -= dt;
      e.color.a = (e.lifeTime / 0.3f) * 0.8f;
      e.scale = glm::vec2(240) * (1.0f + (0.3f - e.lifeTime) * 2.0f); // Expand
      if (e.lifeTime <= 0) {
        entities[i] = entities.back();
        entities.pop_back();
        continue;
      }
      // Deal AoE damage once (on spawn frame)
      if (e.penetration > 0) {
        for (int j = 1; j < (int)entities.size(); ++j) {
          Entity &tgt = entities[j];
          bool isEnemy = (tgt.type == EntityType::Blob ||
                          tgt.type == EntityType::Skeleton ||
                          tgt.type == EntityType::SkeletonMage);
          if (!isEnemy)
            continue;
          if (glm::distance(e.position, tgt.position) > e.radius)
            continue;
          tgt.hp -= e.damage;
          perf.windowDmgDealt += e.damage;
          perf.totalDmgDealt += e.damage;
          if (tgt.hp <= 0) {
            HandleEnemyDeath(j);
            if (j <= i)
              --i;
            --j;
          }
        }
        e.penetration = 0; // Only damage once
      }
    }

    // --- Collections ---
    if (i >= (int)entities.size())
      continue;
    Entity &ec = entities[i];
    if (ec.type == EntityType::Crystal) {
      if (CheckCollisionAABB(*player, ec)) {
        xp++;
        score++;
        perf.windowXp++;
        perf.totalXp++;
        audio.PlaySFX(SFXType::Collect);
        entities[i] = entities.back();
        entities.pop_back();
        if (xp >= xpToNextLevel)
          TriggerLevelUp();
      }
    } else if (ec.type == EntityType::HealthGem) {
      if (CheckCollisionAABB(*player, ec)) {
        player->hp = std::min(player->hp + 20, player->maxHp);
        audio.PlaySFX(SFXType::Collect);
        entities[i] = entities.back();
        entities.pop_back();
      }
    } else if (ec.type == EntityType::PiercingGem) {
      if (CheckCollisionAABB(*player, ec)) {
        player->piercingTimer = 5;
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
      if (!dodging && CheckCollisionAABB(*player, ed)) {
        float dmg = ed.contactDamage * dt;
        player->hp -= dmg;
        perf.windowDmgTaken += dmg;
        perf.totalDmgTaken += dmg;
        perf.timeSinceLastHit = 0;
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
              e.position += (d / dist2) * (rSum - dist2) * 0.3f;
            }
          }
        }
    }
  }
}

bool Game::CheckCollisionAABB(const Entity &a, const Entity &b) {
  // AABB check: |center_dist| < (size_a/2 + size_b/2)
  bool collisionX = std::abs(a.position.x - b.position.x) * 2 <
                    (a.colliderSize.x + b.colliderSize.x);
  bool collisionY = std::abs(a.position.y - b.position.y) * 2 <
                    (a.colliderSize.y + b.colliderSize.y);
  return collisionX && collisionY;
}

int Game::GetRenderLayer(EntityType t) const {
  switch (t) {
  case EntityType::SkeletonCorpse:
  case EntityType::BlobCorpse:
    return 0; // Bottom layer
  case EntityType::Crystal:
  case EntityType::HealthGem:
  case EntityType::PiercingGem:
    return 1; // Pickups above corpses
  case EntityType::Blob:
  case EntityType::Skeleton:
  case EntityType::SkeletonMage:
    return 2; // Enemies on top of pickups
  case EntityType::SwordSwing:
  case EntityType::Explosion:
    return 3; // Effects above enemies
  case EntityType::Bullet:
  case EntityType::EnemyBullet:
    return 4; // Bullets above everything
  case EntityType::Player:
    return 5; // Player always on top
  default:
    return 1;
  }
}

// ===================================================================
// DIFFICULTY DIRECTOR IMPLEMENTATION
// Comprehensive performance-based adaptive difficulty system
// ===================================================================

void Game::UpdatePerformanceMetrics(float dt) {
  // Track time since last hit
  perf.timeSinceLastHit += dt;
  if (perf.timeSinceLastHit > perf.longestNoHitStreak)
    perf.longestNoHitStreak = perf.timeSinceLastHit;

  // Track average HP percentage (exponential moving average)
  if (player) {
    float hpPct = player->hp / std::max(1.0f, player->maxHp);
    perf.avgHpPercent = perf.avgHpPercent * 0.99f + hpPct * 0.01f;
  }

  // Rolling window: accumulate for WINDOW_DURATION, then update smoothed rates
  perf.windowTimer += dt;
  if (perf.windowTimer >= PerformanceMetrics::WINDOW_DURATION) {
    float dur = perf.windowTimer;
    // Exponential moving average: 70% old + 30% new window
    perf.killsPerSecond =
        perf.killsPerSecond * 0.7f + (perf.windowKills / dur) * 0.3f;
    perf.damageTakenPerSecond =
        perf.damageTakenPerSecond * 0.7f + (perf.windowDmgTaken / dur) * 0.3f;
    perf.damageDealtPerSecond =
        perf.damageDealtPerSecond * 0.7f + (perf.windowDmgDealt / dur) * 0.3f;
    perf.xpPerSecond = perf.xpPerSecond * 0.7f + (perf.windowXp / dur) * 0.3f;

    // Reset window accumulators
    perf.windowKills = 0;
    perf.windowDmgTaken = 0;
    perf.windowDmgDealt = 0;
    perf.windowXp = 0;
    perf.windowTimer = 0;
  }

  // Performance history: sample every 3 seconds
  perf.historyTimer += dt;
  if (perf.historyTimer >= 3.0f) {
    perf.historyTimer = 0;
    float score = CalculatePerformanceScore();
    perf.performanceHistory[perf.historyIndex] = score;
    perf.historyIndex = (perf.historyIndex + 1) % 10;
    if (perf.historySamples < 10)
      perf.historySamples++;
  }
}

float Game::CalculateOffensivePower() const {
  // DPS from bullets: damage / fireCooldown * penetration bonus
  float effectiveFireRate = 1.0f / std::max(0.05f, fireCooldown);
  float rawDps = bulletDamage * effectiveFireRate;

  // Penetration multiplier: each extra pierce adds 30% effective DPS
  float pierceBonus = 1.0f + (bulletPenetration - 1) * 0.3f;

  // Piercing gem uptime bonus (temporary power spikes)
  float piercingBonus = 1.0f;
  if (player && player->piercingTimer > 0)
    piercingBonus = 1.3f;

  float totalDps = rawDps * pierceBonus * piercingBonus;

  // Normalize: baseline DPS is 75 (15 dmg * 5 shots/s)
  return totalDps / 75.0f;
}

float Game::CalculateDefensivePower() const {
  if (!player)
    return 1.0f;

  // HP pool: how much total damage can be absorbed
  float hpFactor = player->maxHp / 100.0f;

  // Current HP matters: full health = can afford more risk
  float currentHpBonus = 1.0f;
  float hpPct = player->hp / std::max(1.0f, player->maxHp);
  if (hpPct > 0.8f)
    currentHpBonus = 1.1f; // High HP = slight boost
  else if (hpPct < 0.3f)
    currentHpBonus = 0.7f; // Low HP = reduce pressure
  else if (hpPct < 0.5f)
    currentHpBonus = 0.85f;

  // Dodge availability: having dodge ready is defensive power
  float dodgeBonus = 1.0f;
  if (dodgeTimer >= dodgeCooldown)
    dodgeBonus = 1.1f; // Dodge ready
  if (dodging)
    dodgeBonus = 1.2f; // Currently invincible

  return hpFactor * currentHpBonus * dodgeBonus;
}

float Game::CalculateMobilityPower() const {
  // Speed advantage: faster player can dodge more
  float speedRatio = playerSpeed / 300.0f;

  // Dodge roll gives extra mobility
  float dodgeMobilityBonus = 1.0f + (dodgeSpeed / 1200.0f - 1.0f) * 0.2f;

  return speedRatio * dodgeMobilityBonus;
}

float Game::CalculatePlayerPower() const {
  // Weighted combination of all power components
  float offensive = CalculateOffensivePower();
  float defensive = CalculateDefensivePower();
  float mobility = CalculateMobilityPower();

  // Offensive power matters most (50%), defensive (30%), mobility (20%)
  float rawPower = offensive * 0.50f + defensive * 0.30f + mobility * 0.20f;

  // Level bonus: each level adds ~3% effective power
  float levelBonus = 1.0f + playerLevel * 0.03f;

  return rawPower * levelBonus;
}

float Game::CalculatePerformanceScore() const {
  // Performance score: how well is the player actually doing?
  // 1.0 = balanced, >1 = dominating, <1 = struggling
  float score = 1.0f;

  // Factor 1: Kill rate relative to expected
  // Expected: ~2 kills/sec at baseline
  float expectedKps = 2.0f;
  float killRatio = perf.killsPerSecond / std::max(0.1f, expectedKps);
  // Clamp to [0.2, 3.0] to avoid extreme values
  killRatio = std::clamp(killRatio, 0.2f, 3.0f);

  // Factor 2: Damage efficiency (dealing vs taking)
  float damageEfficiency = 1.0f;
  if (perf.damageTakenPerSecond > 0.01f) {
    float ratio = perf.damageDealtPerSecond / perf.damageTakenPerSecond;
    // High ratio = player is dominating
    damageEfficiency = std::clamp(ratio / 10.0f, 0.3f, 2.5f);
  } else if (perf.damageDealtPerSecond > 1.0f) {
    // Taking zero damage but dealing lots = clearly dominating
    damageEfficiency = 2.0f;
  }

  // Factor 3: HP health status
  float healthScore = 1.0f;
  if (perf.avgHpPercent > 0.9f)
    healthScore = 1.3f; // Almost full HP = too easy
  else if (perf.avgHpPercent > 0.7f)
    healthScore = 1.1f;
  else if (perf.avgHpPercent > 0.5f)
    healthScore = 1.0f; // Balanced
  else if (perf.avgHpPercent > 0.3f)
    healthScore = 0.8f; // Getting hurt
  else
    healthScore = 0.5f; // Critical!

  // Factor 4: No-hit streak
  float noHitBonus = 1.0f;
  if (perf.timeSinceLastHit > 15.0f)
    noHitBonus = 1.4f; // Haven't been hit in 15s = too easy
  else if (perf.timeSinceLastHit > 10.0f)
    noHitBonus = 1.2f;
  else if (perf.timeSinceLastHit > 5.0f)
    noHitBonus = 1.1f;
  else if (perf.timeSinceLastHit < 1.0f)
    noHitBonus = 0.7f; // Just got hit

  // Factor 5: XP collection rate (indicates how well player farms)
  float xpScore = 1.0f;
  float expectedXps = 0.8f; // ~1 crystal per 1.2s at baseline
  if (perf.xpPerSecond > expectedXps * 2.0f)
    xpScore = 1.3f;
  else if (perf.xpPerSecond > expectedXps)
    xpScore = 1.1f;
  else if (perf.xpPerSecond < expectedXps * 0.3f && gameTime > 10.0f)
    xpScore = 0.7f;

  // Weighted combination
  score = killRatio * 0.30f + damageEfficiency * 0.25f + healthScore * 0.20f +
          noHitBonus * 0.15f + xpScore * 0.10f;

  return score;
}

WaveDifficultyConfig Game::GetWaveConfig(int wave) const {
  // Each wave has specific difficulty parameters:
  // targetPowerRatio: how hard relative to player power
  // difficultyCap: absolute maximum multiplier
  // rampSpeed: how fast difficulty approaches target
  // difficultyFloor: minimum multiplier
  // spawnRateMult: base spawn rate for this wave
  // bossHpMult: boss HP scaling

  WaveDifficultyConfig configs[4] = {
      // Wave 1: Skeletons — below player power, gentle start
      {0.65f, 1.2f, 0.15f, 0.25f, 0.8f, 0.7f},
      // Wave 2: Slimes — at player power level
      {0.90f, 1.8f, 0.20f, 0.40f, 1.0f, 1.0f},
      // Wave 3: Big Skeletons — slightly above player
      {1.10f, 2.5f, 0.25f, 0.50f, 1.2f, 1.3f},
      // Wave 4: Big Slimes — above player
      {1.30f, 3.2f, 0.30f, 0.60f, 1.4f, 1.6f},
  };

  if (wave >= 0 && wave < 4)
    return configs[wave];

  // Endless: no cap, aggressive ramp
  float endlessTime = endlessTimer;
  return WaveDifficultyConfig{
      1.5f + endlessTime * 0.02f,  // Target grows over time
      999.0f,                      // No cap
      0.40f + endlessTime * 0.01f, // Ramp gets faster
      1.0f,                        // Minimum 1.0x in endless
      1.5f + endlessTime * 0.03f,  // Spawn rate escalates
      2.0f + endlessTime * 0.05f,  // Boss HP escalates
  };
}

float Game::CalculateTargetDifficulty() const {
  WaveDifficultyConfig cfg = GetWaveConfig(endlessMode ? 99 : currentWave);
  float playerPower = CalculatePlayerPower();
  float perfScore = CalculatePerformanceScore();

  // Base target: player power scaled by wave's target ratio
  float baseTarget = playerPower * cfg.targetPowerRatio;

  // Performance adjustment:
  // If player is dominating (perfScore > 1.2), push difficulty higher
  // If player is struggling (perfScore < 0.8), ease off
  float perfAdjustment = 1.0f;
  if (perfScore > 1.5f)
    perfAdjustment = 1.4f; // Clearly dominating
  else if (perfScore > 1.2f)
    perfAdjustment = 1.2f; // Doing well
  else if (perfScore > 0.9f)
    perfAdjustment = 1.0f; // Balanced
  else if (perfScore > 0.7f)
    perfAdjustment = 0.85f; // Struggling slightly
  else if (perfScore > 0.5f)
    perfAdjustment = 0.7f; // Struggling significantly
  else
    perfAdjustment = 0.5f; // About to die

  // Performance trend: check if player is improving or declining
  float trend = 0.0f;
  if (perf.historySamples >= 3) {
    // Compare recent vs older samples
    int recent = (perf.historyIndex - 1 + 10) % 10;
    int older =
        (perf.historyIndex - std::min(perf.historySamples, 5) + 10) % 10;
    float recentScore = perf.performanceHistory[recent];
    float olderScore = perf.performanceHistory[older];
    trend = recentScore - olderScore;
    // Positive trend = player getting stronger, push difficulty up slightly
    // Negative trend = player weakening, ease off slightly
    trend = std::clamp(trend, -0.3f, 0.3f);
  }

  float target = baseTarget * perfAdjustment + trend * 0.2f;

  // Critical safety: if player HP is very low, reduce pressure
  if (player) {
    float hpPct = player->hp / std::max(1.0f, player->maxHp);
    if (hpPct < 0.15f)
      target *= 0.5f; // Emergency ease-off
    else if (hpPct < 0.25f)
      target *= 0.7f;
    else if (hpPct < 0.4f)
      target *= 0.85f;
  }

  // Clamp to wave's floor and ceiling
  target = std::clamp(target, cfg.difficultyFloor, cfg.difficultyCap);

  return target;
}

void Game::UpdateDifficultyDirector(float dt) {
  // Calculate where difficulty should be heading
  targetDifficulty = CalculateTargetDifficulty();
  WaveDifficultyConfig cfg = GetWaveConfig(endlessMode ? 99 : currentWave);

  // Smooth interpolation toward target
  float diff = targetDifficulty - difficultyRating;
  float rampSpeed = cfg.rampSpeed;

  // Asymmetric ramping: faster to reduce difficulty than increase
  // This prevents sudden death spirals while allowing gradual escalation
  if (diff < 0) {
    // Reducing difficulty: move faster (compassion)
    rampSpeed *= 2.5f;
  } else if (diff > 0.5f) {
    // Far from target: slightly faster approach
    rampSpeed *= 1.3f;
  }

  // Approach target smoothly
  if (std::abs(diff) < 0.01f) {
    difficultyRating = targetDifficulty; // Snap when very close
  } else {
    difficultyRating += diff * rampSpeed * dt;
  }

  // Final clamp to wave's absolute bounds
  difficultyRating =
      std::clamp(difficultyRating, cfg.difficultyFloor, cfg.difficultyCap);

  // Minimum game time ramp: even if player is weak, ensure minimal progression
  // This prevents the game from being stuck at trivial difficulty forever
  float timeFloor = 0.3f + gameTime * 0.003f; // Minimum grows by 0.18/min
  if (difficultyRating < timeFloor && !endlessMode)
    difficultyRating = std::min(timeFloor, cfg.difficultyCap);
}

DifficultyOutput Game::CalculateDifficultyOutput() const {
  DifficultyOutput out;
  float base = difficultyRating;
  WaveDifficultyConfig cfg = GetWaveConfig(endlessMode ? 99 : currentWave);

  // == Enemy HP ==
  // HP scales with difficulty but with diminishing returns
  // This prevents enemies from becoming unkillable bullet sponges
  out.enemyHpMult = 0.6f + base * 0.6f;
  if (out.enemyHpMult > 1.5f)
    out.enemyHpMult = 1.5f + (out.enemyHpMult - 1.5f) * 0.4f; // Diminishing
  out.enemyHpMult = std::max(0.3f, out.enemyHpMult);

  // == Enemy Damage ==
  // Damage scales more aggressively than HP (makes game feel dangerous)
  out.enemyDamageMult = 0.5f + base * 0.7f;
  out.enemyDamageMult = std::clamp(out.enemyDamageMult, 0.2f, 4.0f);

  // Safety: if player HP low, reduce damage further
  if (player) {
    float hpPct = player->hp / std::max(1.0f, player->maxHp);
    if (hpPct < 0.2f)
      out.enemyDamageMult *= 0.6f;
    else if (hpPct < 0.4f)
      out.enemyDamageMult *= 0.8f;
  }

  // == Enemy Speed ==
  // Speed scales gently — too fast enemies feel unfair
  out.enemySpeedMult = 0.8f + base * 0.25f;
  // Clamp speed: never below 0.5x, never above 1.8x
  out.enemySpeedMult = std::clamp(out.enemySpeedMult, 0.5f, 1.8f);
  // Add subtle time-based boost (always gets slightly faster)
  out.enemySpeedMult *= 1.0f + gameTime * 0.0005f;
  if (out.enemySpeedMult > 2.0f)
    out.enemySpeedMult = 2.0f;

  // == Spawn Rate ==
  // Spawn rate comes from wave config + difficulty
  out.spawnRateMult = cfg.spawnRateMult * (0.7f + base * 0.4f);
  out.spawnRateMult = std::clamp(out.spawnRateMult, 0.3f, 5.0f);

  // == Boss HP ==
  // Boss HP based on player's offensive power: should take 15-30 seconds
  float offPower = CalculateOffensivePower();
  out.bossHpMult = cfg.bossHpMult * (0.5f + offPower * 0.5f);
  out.bossHpMult = std::max(0.3f, out.bossHpMult);

  // == Boss Damage ==
  out.bossDamageMult = 0.5f + base * 0.6f;
  out.bossDamageMult = std::clamp(out.bossDamageMult, 0.3f, 3.0f);

  return out;
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
  spriteData.reserve(entities.size() + 200);

  // --- Background ground tiles ---
  if (player) {
    const float TILE = 128.0f; // World-space tile size
    // Calculate visible area
    float halfW = (float)width / 2.0f + TILE;
    float halfH = (float)height / 2.0f + TILE;
    float camX = player->position.x;
    float camY = player->position.y;
    // Tile grid range
    int startX = (int)std::floor((camX - halfW) / TILE);
    int endX = (int)std::ceil((camX + halfW) / TILE);
    int startY = (int)std::floor((camY - halfH) / TILE);
    int endY = (int)std::ceil((camY + halfH) / TILE);
    for (int ty = startY; ty <= endY; ++ty) {
      for (int tx = startX; tx <= endX; ++tx) {
        // Checkerboard pattern: alternate between two tile variants
        bool variant = ((tx + ty) & 1) != 0;
        glm::vec2 tileUV = variant ? glm::vec2(0.25f, 0.75f) // tile 2
                                   : glm::vec2(0.0f, 0.75f); // tile 1
        glm::vec2 pos = {tx * TILE + TILE * 0.5f, ty * TILE + TILE * 0.5f};
        spriteData.push_back(
            {pos, {TILE, TILE}, tileUV, {0.25f, 0.25f}, {1, 1, 1, 1}, 0});
      }
    }
  }

  // Sort entities by render layer: corpses < crystals/gems < enemies < bullets
  // < player
  static std::vector<int> renderOrder;
  renderOrder.resize(entities.size());
  for (int i = 0; i < (int)entities.size(); ++i)
    renderOrder[i] = i;
  std::sort(renderOrder.begin(), renderOrder.end(), [this](int a, int b) {
    return GetRenderLayer(entities[a].type) < GetRenderLayer(entities[b].type);
  });

  // Update facing direction based on nearest enemy (before rendering)
  if (player && state == GameState::Playing) {
    float nearestDist = 99999.0f;
    for (int ei = 1; ei < (int)entities.size(); ++ei) {
      auto &en = entities[ei];
      if (en.type == EntityType::Blob || en.type == EntityType::Skeleton ||
          en.type == EntityType::SkeletonMage) {
        float d = glm::distance(en.position, player->position);
        if (d < nearestDist) {
          nearestDist = d;
          facingLeft = (en.position.x < player->position.x);
        }
      }
    }
  }

  for (int idx : renderOrder) {
    const auto &e = entities[idx];
    float solid = (e.type == EntityType::Explosion) ? 1.0f : 0.0f;
    glm::vec2 renderScale = e.scale;
    // Flip player sprite when facing left
    if (idx == 0 && facingLeft) {
      renderScale.x = -renderScale.x;
    }
    spriteData.push_back(
        {e.position, renderScale, e.uvOffset, e.uvScale, e.color, solid});
  }

  // Player Weapon Sprite
  if (player && state == GameState::Playing) {
    glm::vec2 weaponUV;
    glm::vec2 weaponScale;
    glm::vec2 handOffset; // Offset from player center to hand position
    if (currentWeapon == WeaponType::MachineGun) {
      weaponUV = {0, 0.5f};
      weaponScale = {36, 24};
      handOffset = {22, 2};
    } else if (currentWeapon == WeaponType::Sword) {
      weaponUV = {0.25f, 0.5f};
      weaponScale = {28, 44};
      handOffset = {24, -6};
    } else { // Bazooka
      weaponUV = {0.5f, 0.5f};
      weaponScale = {44, 24};
      handOffset = {26, 2};
    }

    // Flip weapon to match facing
    glm::vec2 offset = handOffset;
    glm::vec2 scale = weaponScale;
    if (facingLeft) {
      offset.x = -offset.x;
      scale.x = -scale.x; // Flip sprite horizontally
    }

    glm::vec2 weaponPos = player->position + offset;
    spriteData.push_back(
        {weaponPos, scale, weaponUV, {0.25f, 0.25f}, {1, 1, 1, 1}, 0});
  }

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
    if (player->piercingTimer > 0) {
      DrawText(textData, cam.x - width / 2.0f + 10, cam.y - height / 2.0f + 85,
               "POWER UP! " + std::to_string((int)player->piercingTimer) + "s",
               {0.8f, 0.3f, 1, 1}, 18);
    }
    // Dodge indicator
    if (dodgeTimer >= dodgeCooldown)
      DrawText(textData, cam.x - width / 2.0f + 10, cam.y - height / 2.0f + 108,
               "DODGE READY", {0.3f, 1, 1, 1}, 16);
    else
      DrawText(
          textData, cam.x - width / 2.0f + 10, cam.y - height / 2.0f + 108,
          [&] {
            char b[16];
            snprintf(b, sizeof(b), "DODGE %.1fs", dodgeCooldown - dodgeTimer);
            return std::string(b);
          }(),
          {0.5f, 0.5f, 0.5f, 1}, 16);
    // Wave / Mode display
    if (endlessMode) {
      DrawText(textData, cam.x + width / 2.0f - 180, cam.y - height / 2.0f + 10,
               "ENDLESS", {1, 0.3f, 0.3f, 1}, 22);
      // Show difficulty
      DrawText(textData, cam.x + width / 2.0f - 180, cam.y - height / 2.0f + 35,
               "DIFF:" + std::to_string((int)difficultyRating),
               {1, 0.5f, 0.2f, 1}, 16);
    } else {
      std::string waveNames[] = {"SKELETONS", "SLIMES", "BIG SKELETONS",
                                 "BIG SLIMES"};
      std::string wn = (currentWave < 4) ? waveNames[currentWave] : "???";
      DrawText(textData, cam.x + width / 2.0f - 200, cam.y - height / 2.0f + 10,
               "WAVE " + std::to_string(currentWave + 1) + ": " + wn,
               {0.8f, 0.6f, 1, 1}, 18);
    }
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
  if (state == GameState::WeaponSelect) {
    spriteData.push_back({{cam.x, cam.y},
                          {(float)width, (float)height},
                          {0, 0},
                          {0, 0},
                          {0.05f, 0.02f, 0.1f, 0.9f},
                          1});
    DrawText(textData, cam.x - 200, cam.y - 140, "CHOOSE WEAPON",
             {1, 0.9f, 0.3f, 1}, 40);
    // Machine Gun box
    spriteData.push_back({{cam.x - 220, cam.y + 10},
                          {180, 140},
                          {0, 0},
                          {0, 0},
                          {0.15f, 0.15f, 0.2f, 0.9f},
                          1});
    DrawText(textData, cam.x - 295, cam.y - 40, "[1] MACHINE", {1, 1, 0.3f, 1},
             20);
    DrawText(textData, cam.x - 295, cam.y - 10, "GUN", {1, 1, 0.3f, 1}, 20);
    DrawText(textData, cam.x - 295, cam.y + 20, "Fast fire",
             {0.7f, 0.7f, 0.7f, 1}, 16);
    DrawText(textData, cam.x - 295, cam.y + 45, "DMG:15", {0.5f, 0.8f, 0.5f, 1},
             16);
    // Sword box
    spriteData.push_back({{cam.x, cam.y + 10},
                          {180, 140},
                          {0, 0},
                          {0, 0},
                          {0.15f, 0.15f, 0.2f, 0.9f},
                          1});
    DrawText(textData, cam.x - 75, cam.y - 40, "[2] SWORD", {0.5f, 0.8f, 1, 1},
             20);
    DrawText(textData, cam.x - 75, cam.y - 10, "Melee arc",
             {0.7f, 0.7f, 0.7f, 1}, 16);
    DrawText(textData, cam.x - 75, cam.y + 15, "3x DMG", {0.7f, 0.7f, 0.7f, 1},
             16);
    DrawText(textData, cam.x - 75, cam.y + 45, "DMG:45", {0.5f, 0.8f, 0.5f, 1},
             16);
    // Bazooka box
    spriteData.push_back({{cam.x + 220, cam.y + 10},
                          {180, 140},
                          {0, 0},
                          {0, 0},
                          {0.15f, 0.15f, 0.2f, 0.9f},
                          1});
    DrawText(textData, cam.x + 145, cam.y - 40, "[3] BAZOOKA",
             {1, 0.5f, 0.2f, 1}, 20);
    DrawText(textData, cam.x + 145, cam.y - 10, "Explosive",
             {0.7f, 0.7f, 0.7f, 1}, 16);
    DrawText(textData, cam.x + 145, cam.y + 15, "AoE blast",
             {0.7f, 0.7f, 0.7f, 1}, 16);
    DrawText(textData, cam.x + 145, cam.y + 45, "DMG:60", {0.5f, 0.8f, 0.5f, 1},
             16);
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
