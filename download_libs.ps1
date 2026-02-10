# download_libs.ps1 — Pobiera wgpu-native i GLFW na Windows (odpowiednik download_libs.sh)

# ============================================================
#  KONFIGURACJA
# ============================================================
$WGPU_VERSION = "v0.19.1.1"
$GLFW_VERSION = "3.3.8"

# ============================================================
#  1. wgpu-native
# ============================================================
$WGPU_FILE = "wgpu-windows-x86_64-release.zip"
$WGPU_URL = "https://github.com/gfx-rs/wgpu-native/releases/download/$WGPU_VERSION/$WGPU_FILE"
$WGPU_DIR = "external/wgpu"

Write-Host "=== wgpu-native $WGPU_VERSION ===" -ForegroundColor Cyan

# Tworzenie folderow
New-Item -ItemType Directory -Force -Path "$WGPU_DIR/lib" | Out-Null
New-Item -ItemType Directory -Force -Path "$WGPU_DIR/include/webgpu" | Out-Null

# Pobieranie
$zipPath = "$WGPU_DIR/$WGPU_FILE"
Write-Host "Pobieranie $WGPU_FILE ..." -ForegroundColor Yellow
try {
    Invoke-WebRequest -Uri $WGPU_URL -OutFile $zipPath -UseBasicParsing
}
catch {
    Write-Host "BLAD: Nie udalo sie pobrac wgpu-native!" -ForegroundColor Red
    Write-Host $_.Exception.Message
    exit 1
}
Write-Host "Pobrano!" -ForegroundColor Green

# Rozpakowywanie
Expand-Archive -Path $zipPath -DestinationPath $WGPU_DIR -Force

# Reorganizacja
Get-ChildItem "$WGPU_DIR" -Filter "*.dll" -File | Move-Item -Destination "$WGPU_DIR/lib/" -Force
Get-ChildItem "$WGPU_DIR" -Filter "*.lib" -File | Move-Item -Destination "$WGPU_DIR/lib/" -Force
Get-ChildItem "$WGPU_DIR" -Filter "*.a"   -File | Move-Item -Destination "$WGPU_DIR/lib/" -Force
Get-ChildItem "$WGPU_DIR" -Filter "*.h"   -File | Move-Item -Destination "$WGPU_DIR/include/webgpu/" -Force

Remove-Item $zipPath -Force
Write-Host "  -> $WGPU_DIR/lib/        (biblioteki)" -ForegroundColor Gray
Write-Host "  -> $WGPU_DIR/include/    (headery)" -ForegroundColor Gray
Write-Host ""

# ============================================================
#  2. GLFW
# ============================================================
$GLFW_FILE = "glfw-$GLFW_VERSION.bin.WIN64.zip"
$GLFW_URL = "https://github.com/glfw/glfw/releases/download/$GLFW_VERSION/$GLFW_FILE"
$GLFW_DIR = "external/glfw"

Write-Host "=== GLFW $GLFW_VERSION ===" -ForegroundColor Cyan

# Tworzenie folderow
New-Item -ItemType Directory -Force -Path "$GLFW_DIR/lib" | Out-Null
New-Item -ItemType Directory -Force -Path "$GLFW_DIR/include" | Out-Null

# Pobieranie
$glfwZipPath = "$GLFW_DIR/$GLFW_FILE"
Write-Host "Pobieranie $GLFW_FILE ..." -ForegroundColor Yellow
try {
    Invoke-WebRequest -Uri $GLFW_URL -OutFile $glfwZipPath -UseBasicParsing
}
catch {
    Write-Host "BLAD: Nie udalo sie pobrac GLFW!" -ForegroundColor Red
    Write-Host $_.Exception.Message
    exit 1
}
Write-Host "Pobrano!" -ForegroundColor Green

# Rozpakowywanie (zip ma folder glfw-3.3.8.bin.WIN64/ w srodku)
$tempDir = "$GLFW_DIR/_temp"
Expand-Archive -Path $glfwZipPath -DestinationPath $tempDir -Force

$innerDir = "$tempDir/glfw-$GLFW_VERSION.bin.WIN64"

# Kopiowanie headerow (include/GLFW/glfw3.h, glfw3native.h)
Copy-Item -Path "$innerDir/include/*" -Destination "$GLFW_DIR/include/" -Recurse -Force

# Kopiowanie bibliotek z lib-vc2022 (MSVC — pasuje do naszego Build Tools)
Copy-Item -Path "$innerDir/lib-vc2022/*" -Destination "$GLFW_DIR/lib/" -Recurse -Force

# Czyszczenie
Remove-Item $glfwZipPath -Force
Remove-Item $tempDir -Recurse -Force

Write-Host "  -> $GLFW_DIR/lib/        (biblioteki: glfw3.lib, glfw3.dll)" -ForegroundColor Gray
Write-Host "  -> $GLFW_DIR/include/    (headery: GLFW/glfw3.h)" -ForegroundColor Gray
Write-Host ""

# ============================================================
Write-Host "Wszystkie biblioteki pobrane!" -ForegroundColor Green
