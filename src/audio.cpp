#include "audio.h"
#include <iostream>

bool AudioSystem::Init() {
  ma_engine_config config = ma_engine_config_init();
  if (ma_engine_init(&config, &engine) != MA_SUCCESS) {
    std::cerr << "[Audio] Failed to initialize audio engine" << std::endl;
    return false;
  }
  initialized = true;
  std::cout << "[Audio] Engine initialized" << std::endl;
  return true;
}

void AudioSystem::PlaySFX(SFXType type) {
  if (!initialized)
    return;

  // Per-type cooldown in milliseconds
  int cooldownMs = 0;
  switch (type) {
  case SFXType::Collect:
    cooldownMs = 250;
    break;
  case SFXType::Hit:
    cooldownMs = 200;
    break;
  case SFXType::Shoot:
    cooldownMs = 150;
    break;
  default:
    cooldownMs = 0; // LevelUp, Death — no throttle
    break;
  }

  // Throttle check
  if (cooldownMs > 0) {
    auto now = Clock::now();
    int key = static_cast<int>(type);
    auto it = lastPlayTime.find(key);
    if (it != lastPlayTime.end()) {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - it->second)
                         .count();
      if (elapsed < cooldownMs)
        return; // Too soon, skip
    }
    lastPlayTime[key] = now;
  }

  const char *path = nullptr;
  switch (type) {
  case SFXType::Shoot:
    path = "assets/sfx/shoot.wav";
    break;
  case SFXType::Hit:
    path = "assets/sfx/hit.wav";
    break;
  case SFXType::Collect:
    path = "assets/sfx/collect.wav";
    break;
  case SFXType::LevelUp:
    path = "assets/sfx/levelup.wav";
    break;
  case SFXType::Death:
    path = "assets/sfx/death.wav";
    break;
  }
  if (path) {
    ma_engine_play_sound(&engine, path, nullptr);
  }
}

void AudioSystem::Cleanup() {
  if (initialized) {
    ma_engine_uninit(&engine);
    initialized = false;
  }
}
