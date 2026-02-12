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
  PiercingGem
};

// Game States
enum class GameState { Playing, LevelUp, GameOver };

// Upgrade Types
enum class UpgradeType {
  MaxHP,      // +20 Max HP & Heal
  Damage,     // +5 Bullet Damage
  FireRate,   // -0.05s Cooldown
  Speed,      // +50 Move Speed
  Penetration // +2 Penetration (permanent)
};

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
  float lifeTime = 0.0f;
  int penetration = 1;
  float piercingTimer = 0.0f;
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
  void SpawnBullet(glm::vec2 targetPos);
  void SpawnGem(glm::vec2 pos, int type);
  int FindNearestEnemy();

  // Level Up
  void TriggerLevelUp();
  void ApplyUpgrade(int choice);
  std::vector<Upgrade> GenerateUpgradeOptions();

  // Text Rendering
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
  WGPUBindGroup fontTexBindGroup = nullptr; // Font texture bind group
  WGPUBuffer vertexBuffer = nullptr;
  WGPUBuffer indexBuffer = nullptr;
  WGPUBuffer instanceBuffer = nullptr;
  WGPUBuffer uniformBuffer = nullptr;

  // Resources
  Texture atlasTexture;
  Texture fontTexture;

  // Audio
  AudioSystem audio;

  // Game State
  struct GLFWwindow *window = nullptr;
  GameState state = GameState::Playing;
  CameraUniforms camUniforms;
  std::vector<Entity> entities;
  Entity *player = nullptr;

  // Progression
  float gameTime = 0.0f;
  float spawnTimer = 0.0f;
  float fireTimer = 0.0f;
  float fireCooldown = 0.2f; // Upgradeable
  int score = 0;
  int xp = 0;
  int xpToNextLevel = 10;
  int playerLevel = 0;
  float playerSpeed = 300.0f; // Upgradeable
  float bulletDamage = 15.0f; // Upgradeable
  int bulletPenetration = 1;  // Upgradeable

  // Level Up Menu
  std::vector<Upgrade> currentUpgrades;
};
