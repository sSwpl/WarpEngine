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
