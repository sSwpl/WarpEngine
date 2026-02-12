#include "texture.h"

#include "stb_image.h"
#include <iostream>


Texture loadTexture(WGPUDevice device, WGPUQueue queue, const char *filePath) {
  Texture result = {};

  // --- 1. Ładowanie pliku obrazu ---
  int width, height, channels;
  // Wymuszamy 4 kanały (RGBA) niezależnie od formatu źródłowego
  unsigned char *pixels = stbi_load(filePath, &width, &height, &channels, 4);
  if (!pixels) {
    std::cerr << "[Texture] Failed to load image: " << filePath << " ("
              << stbi_failure_reason() << ")" << std::endl;
    return result;
  }

  result.width = static_cast<uint32_t>(width);
  result.height = static_cast<uint32_t>(height);

  std::cout << "[Texture] Loaded " << filePath << " (" << width << "x" << height
            << ", " << channels << " ch)" << std::endl;

  // --- 2. Tworzenie WGPUTexture ---
  WGPUTextureDescriptor texDesc = {};
  texDesc.nextInChain = nullptr;
  texDesc.label = "Sprite Texture";
  texDesc.dimension = WGPUTextureDimension_2D;
  texDesc.size = {result.width, result.height, 1};
  texDesc.mipLevelCount = 1;
  texDesc.sampleCount = 1;
  texDesc.format = WGPUTextureFormat_RGBA8Unorm;
  texDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
  texDesc.viewFormatCount = 0;
  texDesc.viewFormats = nullptr;

  result.texture = wgpuDeviceCreateTexture(device, &texDesc);
  if (!result.texture) {
    std::cerr << "[Texture] Failed to create WGPUTexture!" << std::endl;
    stbi_image_free(pixels);
    return result;
  }

  // --- 3. Upload pikseli do GPU ---
  WGPUImageCopyTexture destination = {};
  destination.nextInChain = nullptr;
  destination.texture = result.texture;
  destination.mipLevel = 0;
  destination.origin = {0, 0, 0};
  destination.aspect = WGPUTextureAspect_All;

  WGPUTextureDataLayout dataLayout = {};
  dataLayout.nextInChain = nullptr;
  dataLayout.offset = 0;
  dataLayout.bytesPerRow = 4 * result.width; // RGBA = 4 bajty na piksel
  dataLayout.rowsPerImage = result.height;

  WGPUExtent3D writeSize = {result.width, result.height, 1};
  size_t dataSize = static_cast<size_t>(4) * result.width * result.height;

  wgpuQueueWriteTexture(queue, &destination, pixels, dataSize, &dataLayout,
                        &writeSize);

  // Dane CPU już nie są potrzebne
  stbi_image_free(pixels);

  // --- 4. Tworzenie TextureView ---
  WGPUTextureViewDescriptor viewDesc = {};
  viewDesc.nextInChain = nullptr;
  viewDesc.label = "Sprite Texture View";
  viewDesc.format = WGPUTextureFormat_RGBA8Unorm;
  viewDesc.dimension = WGPUTextureViewDimension_2D;
  viewDesc.baseMipLevel = 0;
  viewDesc.mipLevelCount = 1;
  viewDesc.baseArrayLayer = 0;
  viewDesc.arrayLayerCount = 1;
  viewDesc.aspect = WGPUTextureAspect_All;

  result.view = wgpuTextureCreateView(result.texture, &viewDesc);

  // --- 5. Tworzenie Samplera ---
  WGPUSamplerDescriptor samplerDesc = {};
  samplerDesc.nextInChain = nullptr;
  samplerDesc.label = "Sprite Sampler";
  samplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
  samplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
  samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
  samplerDesc.magFilter = WGPUFilterMode_Nearest; // pixel-art style
  samplerDesc.minFilter = WGPUFilterMode_Nearest;
  samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
  samplerDesc.lodMinClamp = 0.0f;
  samplerDesc.lodMaxClamp = 1.0f;
  samplerDesc.compare = WGPUCompareFunction_Undefined;
  samplerDesc.maxAnisotropy = 1;

  result.sampler = wgpuDeviceCreateSampler(device, &samplerDesc);

  std::cout << "[Texture] GPU texture ready." << std::endl;
  return result;
}

void releaseTexture(Texture &tex) {
  if (tex.sampler) {
    wgpuSamplerRelease(tex.sampler);
    tex.sampler = nullptr;
  }
  if (tex.view) {
    wgpuTextureViewRelease(tex.view);
    tex.view = nullptr;
  }
  if (tex.texture) {
    wgpuTextureDestroy(tex.texture);
    wgpuTextureRelease(tex.texture);
    tex.texture = nullptr;
  }
  tex.width = 0;
  tex.height = 0;
}
