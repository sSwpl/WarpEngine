#pragma once
#include "miniaudio.h"
#include <chrono>
#include <unordered_map>

enum class SFXType { Shoot, Hit, Collect, LevelUp, Death };

class AudioSystem {
public:
  bool Init();
  void PlaySFX(SFXType type);
  void Cleanup();

private:
  ma_engine engine;
  bool initialized = false;

  // Throttling: minimum interval between plays of same SFX type
  using Clock = std::chrono::steady_clock;
  std::unordered_map<int, Clock::time_point> lastPlayTime;
};
