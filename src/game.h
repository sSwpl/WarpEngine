#pragma once
#include "audio.h"
#include "texture.h"
#include <algorithm>
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
  BlobCorpse,     // Dead blob (slime)
  SwordSwing,     // Melee arc attack
  Explosion       // Bazooka AoE
};

// Weapon Types
enum class WeaponType { MachineGun, Sword, Bazooka };

// Game States
enum class GameState { WeaponSelect, Playing, LevelUp, GameOver };

// Upgrade Types
enum class UpgradeType { MaxHP, Damage, FireRate, Speed, Penetration };

struct Upgrade {
  UpgradeType type;
  std::string name;
  glm::vec4 color;
};

// Animation Data
struct Animation {
  int startFrameX = 0;
  int startFrameY = 0;
  int frameCount = 1;
  float frameDuration = 0.1f;
  float timer = 0.0f;
  int currentFrame = 0;
};

// Entity (ECS-lite)
struct Entity {
  bool active = true;
  EntityType type;
  glm::vec2 position;
  glm::vec2 velocity;
  float radius = 10.0f;
  glm::vec2 colliderSize = {40.0f, 40.0f};
  float hp = 10.0f;
  float maxHp = 10.0f;
  float damage = 10.0f;
  float contactDamage = 20.0f;
  float speed = 100.0f;
  float lifeTime = 0.0f;
  int penetration = 1;
  float piercingTimer = 0.0f;
  // Boss timers
  float shootTimer = 0.0f;
  float summonTimer = 0.0f;
  // Lunge (unused)
  float lungeTimer = 0.0f;
  bool lunging = false;
  float lungeDuration = 0.0f;
  // Render
  glm::vec2 scale;
  glm::vec2 uvOffset;
  glm::vec2 uvScale;
  glm::vec4 color;
  Animation anim;
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

// ===================================================================
// DIFFICULTY DIRECTOR — Performance-based adaptive difficulty system
// ===================================================================

// Rolling-window performance metrics
struct PerformanceMetrics {
  // Smoothed rates (exponential moving average)
  float killsPerSecond = 0.0f;       // How fast player kills enemies
  float damageTakenPerSecond = 0.0f; // How much damage player takes/s
  float damageDealtPerSecond = 0.0f; // How much damage player deals/s
  float xpPerSecond = 0.0f;          // XP gain rate
  float avgHpPercent = 1.0f;         // Average HP% (smoothed)

  // Accumulation window (5s rolling)
  float windowKills = 0;
  float windowDmgTaken = 0;
  float windowDmgDealt = 0;
  float windowXp = 0;
  float windowTimer = 0;
  static constexpr float WINDOW_DURATION = 5.0f;

  // Lifetime stats
  float totalKills = 0;
  float totalDmgDealt = 0;
  float totalDmgTaken = 0;
  float totalXp = 0;
  int totalDodgesUsed = 0;
  float timeSinceLastHit = 0.0f; // Seconds since player last took damage
  float longestNoHitStreak = 0.0f;

  // Recent performance history (sampled every 3s)
  float historyTimer = 0.0f;
  float performanceHistory[10] = {}; // Last 10 samples (30s window)
  int historyIndex = 0;
  int historySamples = 0;
};

// Per-wave difficulty configuration
struct WaveDifficultyConfig {
  float targetPowerRatio; // Target difficulty as ratio of player power
  float difficultyCap;    // Hard ceiling for this wave's difficulty multiplier
  float rampSpeed;        // How fast difficulty approaches target (units/sec)
  float difficultyFloor;  // Minimum difficulty for this wave
  float spawnRateMult;    // Base spawn rate multiplier for this wave
  float bossHpMult;       // Boss HP multiplier for this wave
};

// Output of the difficulty system — separate multipliers for each aspect
struct DifficultyOutput {
  float enemyHpMult = 1.0f;
  float enemyDamageMult = 1.0f;
  float enemySpeedMult = 1.0f;
  float spawnRateMult = 1.0f;
  float bossHpMult = 1.0f;
  float bossDamageMult = 1.0f;
};

// ===================================================================

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
  void SpawnEnemy(int enemyType);
  void SpawnBoss(int bossType);
  void SpawnBullet(glm::vec2 targetPos);
  void SpawnEnemyBullet(glm::vec2 from, glm::vec2 target);
  void SpawnGem(glm::vec2 pos, int type);
  void HandleEnemyDeath(int entityIdx); // Crystal/corpse drops
  int FindNearestEnemy();
  bool CheckCollisionAABB(const Entity &a, const Entity &b);

  // Level Up
  void TriggerLevelUp();
  void ApplyUpgrade(int choice);
  std::vector<Upgrade> GenerateUpgradeOptions();

  // Text
  void DrawText(std::vector<InstanceData> &data, float x, float y,
                const std::string &text, glm::vec4 color,
                float charSize = 16.0f);

  // Render order
  int GetRenderLayer(EntityType t) const;

  // === Difficulty Director ===
  void UpdatePerformanceMetrics(float dt);
  void UpdateDifficultyDirector(float dt);
  float CalculatePlayerPower() const;
  float CalculateOffensivePower() const;
  float CalculateDefensivePower() const;
  float CalculateMobilityPower() const;
  float CalculatePerformanceScore() const;
  float CalculateTargetDifficulty() const;
  DifficultyOutput CalculateDifficultyOutput() const;
  WaveDifficultyConfig GetWaveConfig(int wave) const;

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
  WeaponType currentWeapon = WeaponType::MachineGun;
  bool facingLeft = false;

  // Wave system (4 waves + endless)
  int currentWave = 0;
  float waveTimer = 0.0f;
  float waveDuration = 35.0f;
  bool waveBossSpawned = false;
  bool waveBossAlive = false;
  bool endlessMode = false;
  float endlessTimer = 0.0f;

  // Difficulty Director state
  float difficultyRating = 0.3f; // Current difficulty multiplier (starts low)
  float targetDifficulty = 0.3f; // Where difficulty is headed
  PerformanceMetrics perf;       // Performance tracking
  DifficultyOutput diffOut;      // Current frame's output multipliers

  // Dodge roll
  float dodgeCooldown = 2.0f;
  float dodgeTimer = 0.0f;
  bool dodging = false;
  float dodgeDuration = 0.15f;
  float dodgeTimeLeft = 0.0f;
  glm::vec2 dodgeDir = {0, 0};
  float dodgeSpeed = 1200.0f;
  glm::vec2 lastMoveDir = {1, 0};

  // Level Up Menu
  std::vector<Upgrade> currentUpgrades;
};
