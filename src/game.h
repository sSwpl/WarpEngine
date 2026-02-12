#pragma once

#include "texture.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <webgpu/webgpu.h>


// Typy encji
enum class EntityType {
  Player,
  Blob,
  Skeleton,
  Crystal,
  Bullet,
  HealthGem,  // New
  PiercingGem // New
};

// Stan Gry
enum class GameState { Playing, GameOver };

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
  float maxHp = 10.0f;
  float damage = 10.0f;
  float lifeTime = 0.0f;

  // Combat Stats (New)
  int penetration = 1;        // For bullets
  float piercingTimer = 0.0f; // For player (buff duration)

  // Renderowanie
  glm::vec2 scale;
  glm::vec2 uvOffset;
  glm::vec2 uvScale;
  glm::vec4 color;
};

struct InstanceData {
  glm::vec2 position;
  glm::vec2 scale;
  glm::vec2 uvOffset;
  glm::vec2 uvScale;
  glm::vec4 color;
  float useSolidColor;
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
  void ResetGame();
  void ProcessInput(float dt);
  void Update(float dt);
  void Render();
  void RenderUI();
  void Cleanup();

  // Spawning & Logic
  void SpawnEnemy();
  void SpawnBullet(glm::vec2 targetPos);
  void SpawnGem(glm::vec2 pos, int type); // New (0=Green, 1=Purple)
  int FindNearestEnemy();

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
  GameState state = GameState::Playing;
  CameraUniforms camUniforms;
  std::vector<Entity> entities;
  Entity *player = nullptr;

  // Progression & Logic Stats
  float gameTime = 0.0f;
  float spawnTimer = 0.0f;
  float fireTimer = 0.0f;
  int score = 0;

  // Physics Config
  const float PLAYER_SPEED = 300.0f;
};
