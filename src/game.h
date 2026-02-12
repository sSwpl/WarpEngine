#pragma once

#include "texture.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <webgpu/webgpu.h>


// Typy encji
enum class EntityType { Player, Blob, Skeleton, Crystal, Bullet };

// Struktura Encji (ECS-lite)
struct Entity {
  bool active = true;
  EntityType type;

  // Fizyka
  glm::vec2 position;
  glm::vec2 velocity;
  float radius;

  // Gameplay
  float hp = 10.0f;
  float damage = 10.0f;
  float lifeTime = 0.0f; // For bullets (despawn timer)

  // Renderowanie
  glm::vec2 scale;
  glm::vec2 uvOffset;
  glm::vec2 uvScale;
  glm::vec4 color; // Tint (RGBA)
};

struct InstanceData {
  glm::vec2 position;
  glm::vec2 scale;
  glm::vec2 uvOffset;
  glm::vec2 uvScale;
  glm::vec4 color; // New: Color Tint
};

struct CameraUniforms {
  glm::mat4 viewProj;
};

class Game {
public:
  Game();
  ~Game();

  bool Initialize();
  void Run();

private:
  void InitGraphics();
  void InitGame();
  void ProcessInput(float dt);
  void Update(float dt);
  void Render();
  void Cleanup();

  // Spawning & Logic
  void SpawnEnemy();
  void SpawnBullet(glm::vec2 targetPos);
  int FindNearestEnemy(); // Returns index or -1

  // WebGPU Context
  WGPUInstance instance = nullptr;
  WGPUSurface surface = nullptr;
  WGPUAdapter adapter = nullptr;
  WGPUDevice device = nullptr;
  WGPUQueue queue = nullptr;
  WGPUSurfaceConfiguration surfConfig = {};
  int width = 1280;
  int height = 720;

  // Render Pipeline
  WGPURenderPipeline pipeline = nullptr;
  WGPUBindGroup camBindGroup = nullptr;
  WGPUBindGroup texBindGroup = nullptr;
  WGPUBuffer vertexBuffer = nullptr;
  WGPUBuffer indexBuffer = nullptr;
  WGPUBuffer instanceBuffer = nullptr;
  WGPUBuffer uniformBuffer = nullptr;

  // Resources
  Texture atlasTexture;

  // Game State
  struct GLFWwindow *window = nullptr;
  CameraUniforms camUniforms;
  std::vector<Entity> entities;
  Entity *player = nullptr;

  // Progression & Logic Stats
  float gameTime = 0.0f;
  float spawnTimer = 0.0f;
  float fireTimer = 0.0f; // Weapon Cooldown
  int score = 0;

  // Physics Config
  const float PLAYER_SPEED = 300.0f;
};
