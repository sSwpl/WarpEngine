#pragma once

#include "audio.h"
#include "texture.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <webgpu/webgpu.h>

// Entity Types
enum class EntityType {
  Player,
  Blob,
  Skeleton,
  Crystal,
  Bullet,
  HealthGem,
  PiercingGem,
  SkeletonMage,   // Boss
  EnemyBullet,    // Boss projectile
  SkeletonCorpse, // Dead skeleton (bones)
  BlobCorpse      // Dead blob (slime)
};

// Game States
enum class GameState { Playing, LevelUp, GameOver };

// Upgrade Types
enum class UpgradeType { MaxHP, Damage, FireRate, Speed, Penetration };

struct Upgrade {
  UpgradeType type;
  std::string name;
  glm::vec4 color;
};

// Entity (ECS-lite)
struct Entity {
  bool active = true;
  EntityType type;
  glm::vec2 position;
  glm::vec2 velocity;
  float radius = 10.0f;
  float hp = 10.0f;
  float maxHp = 10.0f;
  float damage = 10.0f;
  float speed = 100.0f; // Movement speed
  float lifeTime = 0.0f;
  int penetration = 1;
  float piercingTimer = 0.0f;
  // Boss timers
  float shootTimer = 0.0f;
  float summonTimer = 0.0f;
  // Render
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
  void Cleanup();

  // Spawning
  void SpawnEnemy();
  void SpawnBoss();
  void SpawnBullet(glm::vec2 targetPos);
  void SpawnEnemyBullet(glm::vec2 from, glm::vec2 target);
  void SpawnGem(glm::vec2 pos, int type);
  int FindNearestEnemy();

  // Level Up
  void TriggerLevelUp();
  void ApplyUpgrade(int choice);
  std::vector<Upgrade> GenerateUpgradeOptions();

  // Text
  void DrawText(std::vector<InstanceData> &data, float x, float y,
                const std::string &text, glm::vec4 color,
                float charSize = 16.0f);

  // WebGPU
  WGPUInstance instance = nullptr;
  WGPUSurface surface = nullptr;
  WGPUAdapter adapter = nullptr;
  WGPUDevice device = nullptr;
  WGPUQueue queue = nullptr;
  WGPUSurfaceConfiguration surfConfig = {};
  int width = 1280;
  int height = 720;

  WGPURenderPipeline pipeline = nullptr;
  WGPUBindGroup camBindGroup = nullptr;
  WGPUBindGroup texBindGroup = nullptr;
  WGPUBindGroup fontTexBindGroup = nullptr;
  WGPUBuffer vertexBuffer = nullptr;
  WGPUBuffer indexBuffer = nullptr;
  WGPUBuffer instanceBuffer = nullptr;
  WGPUBuffer uniformBuffer = nullptr;

  Texture atlasTexture;
  Texture fontTexture;
  AudioSystem audio;

  struct GLFWwindow *window = nullptr;
  GameState state = GameState::Playing;
  CameraUniforms camUniforms;
  std::vector<Entity> entities;
  Entity *player = nullptr;

  // Progression
  float gameTime = 0.0f;
  float spawnTimer = 0.0f;
  float fireTimer = 0.0f;
  float fireCooldown = 0.2f;
  int score = 0;
  int xp = 0;
  int xpToNextLevel = 10;
  int playerLevel = 0;
  float playerSpeed = 300.0f;
  float bulletDamage = 15.0f;
  int bulletPenetration = 1;

  // Boss (recurring mini-boss)
  float nextBossTime = 60.0f; // First at 60s, then every ~45s (decreasing)

  // Level Up Menu
  std::vector<Upgrade> currentUpgrades;
};
