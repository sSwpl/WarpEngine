# download_libs.ps1 — Pobiera wgpu-native na Windows (odpowiednik download_libs.sh)

# Konfiguracja
$WGPU_VERSION = "v0.19.1.1"
$FILE = "wgpu-windows-x86_64-release.zip"
$URL = "https://github.com/gfx-rs/wgpu-native/releases/download/$WGPU_VERSION/$FILE"
$TARGET_DIR = "external/wgpu"

Write-Host "=== wgpu-native Downloader (Windows) ===" -ForegroundColor Cyan
Write-Host "Wersja:  $WGPU_VERSION"
Write-Host "Plik:    $FILE"
Write-Host "URL:     $URL"
Write-Host ""

# Tworzenie folderow
New-Item -ItemType Directory -Force -Path "$TARGET_DIR/lib" | Out-Null
New-Item -ItemType Directory -Force -Path "$TARGET_DIR/include/webgpu" | Out-Null

# Pobieranie
$zipPath = "$TARGET_DIR/$FILE"
Write-Host "Pobieranie..." -ForegroundColor Yellow
try {
    Invoke-WebRequest -Uri $URL -OutFile $zipPath -UseBasicParsing
} catch {
    Write-Host "BLAD: Nie udalo sie pobrac pliku!" -ForegroundColor Red
    Write-Host $_.Exception.Message
    exit 1
}
Write-Host "Pobrano!" -ForegroundColor Green

# Rozpakowywanie
Write-Host "Rozpakowywanie..." -ForegroundColor Yellow
Expand-Archive -Path $zipPath -DestinationPath $TARGET_DIR -Force
Write-Host "Rozpakowano!" -ForegroundColor Green

# Reorganizacja — przeniesienie bibliotek do lib/
Get-ChildItem "$TARGET_DIR" -Filter "*.dll" -File | Move-Item -Destination "$TARGET_DIR/lib/" -Force
Get-ChildItem "$TARGET_DIR" -Filter "*.lib" -File | Move-Item -Destination "$TARGET_DIR/lib/" -Force
Get-ChildItem "$TARGET_DIR" -Filter "*.a"   -File | Move-Item -Destination "$TARGET_DIR/lib/" -Force

# Reorganizacja — przeniesienie headerow do include/webgpu/
Get-ChildItem "$TARGET_DIR" -Filter "*.h" -File | Move-Item -Destination "$TARGET_DIR/include/webgpu/" -Force

# Czyszczenie — usuwanie zipa
Remove-Item $zipPath -Force

Write-Host ""
Write-Host "wgpu-native zainstalowane w $TARGET_DIR" -ForegroundColor Green
Write-Host "  Headers:   $TARGET_DIR/include/webgpu/" -ForegroundColor Gray
Write-Host "  Libraries: $TARGET_DIR/lib/" -ForegroundColor Gray
