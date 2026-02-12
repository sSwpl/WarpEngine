#pragma once
#include "miniaudio.h"

enum class SFXType { Shoot, Hit, Collect, LevelUp, Death };

class AudioSystem {
public:
  bool Init();
  void PlaySFX(SFXType type);
  void Cleanup();

private:
  ma_engine engine;
  bool initialized = false;
};
