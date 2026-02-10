# WarpEngine

WarpEngine to niskopoziomowy rdzeń silnika graficznego opracowany w standardzie C++20 z wykorzystaniem API WebGPU (wgpu-native). Projekt został zaprojektowany z myślą o wysokiej wydajności i natywnej obsłudze wielu platform, w tym macOS (Apple Silicon/Metal) oraz Windows (x86_64/Direct3D 12/Vulkan).

## Stan projektu
Aktualnie silnik implementuje:
- Cross-platformową inicjalizację instancji, adaptera oraz urządzenia WebGPU.
- Abstrakcję powierzchni (Surface) dla systemów macOS oraz Windows.
- Potok renderowania (Render Pipeline) wykorzystujący shadery WGSL.
- Podstawowy proces renderowania prymitywów (trójkąt) z czyszczeniem bufora koloru.

## Wymagania systemowe
- CMake (wersja 3.20 lub nowsza)
- Kompilator obsługujący standard C++20 (MSVC na Windows, Clang na macOS)
- Git

## Przygotowanie środowiska
Projekt wymaga zewnętrznych bibliotek (wgpu-native oraz GLFW), które nie są przechowywane w repozytorium. Przed pierwszą kompilacją należy uruchomić skrypt instalacyjny odpowiedni dla danego systemu operacyjnego.

### macOS / Linux
Otwórz terminal w głównym katalogu projektu i wykonaj:
chmod +x download_libs.sh
./download_libs.sh

### Windows (PowerShell)
Otwórz PowerShell w głównym katalogu projektu i wykonaj:
.\download_libs.ps1

Skrypty te automatycznie pobiorą binarne wersje bibliotek i umieszczą je w katalogu `external/`, zachowując strukturę wymaganą przez plik `CMakeLists.txt`.

## Kompilacja i uruchomienie
W celu zbudowania projektu należy wykonać następujące kroki:

1. Utworzenie katalogu budowania:
   mkdir build
   cd build

2. Generowanie plików projektu:
   cmake ..

3. Kompilacja:
   cmake --build .

4. Uruchomienie:
   ./WarpEngine (macOS) lub .\WarpEngine.exe (Windows)

## Struktura plików
- `src/main.cpp`: Główna pętla silnika i logika renderowania.
- `src/wgpu_surface.h/cpp`: Cross-platformowa implementacja tworzenia powierzchni.
- `src/wgpu_surface_macos.mm`: Implementacja warstwy Metal dla macOS (Objective-C++).
- `external/`: Biblioteki i pliki nagłówkowe (generowane automatycznie).
