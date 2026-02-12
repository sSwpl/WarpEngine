#pragma once

#include <cstdint>
#include <webgpu/webgpu.h>

// Struktura przechowująca zasoby GPU powiązane z teksturą
struct Texture {
  WGPUTexture texture = nullptr;
  WGPUTextureView view = nullptr;
  WGPUSampler sampler = nullptr;
  uint32_t width = 0;
  uint32_t height = 0;
};

// Ładuje plik obrazu (PNG/JPG) i tworzy z niego teksturę GPU.
// Zwraca Texture ze wszystkimi zasobami gotowymi do użycia w shaderze.
Texture loadTexture(WGPUDevice device, WGPUQueue queue, const char *filePath);

// Zwalnia wszystkie zasoby GPU powiązane z teksturą
void releaseTexture(Texture &tex);
