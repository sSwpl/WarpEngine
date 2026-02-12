Phase 1: Fundamenty Graficzne 🎨

Cel: Przejście od hardcodowanego trójkąta do wyświetlania tekstur i obsługi kamery 2D.

[x] Integracja biblioteki matematycznej (GLM)

[x] Dodanie glm do projektu (przez CMake FetchContent).

[x] Implementacja macierzy projekcji (Orthographic Projection) dla 2D.

[x] System Tekstur

[x] Integracja stb_image do ładowania plików PNG/JPG.

[x] Implementacja funkcji przesyłania danych obrazu do WGPUTexture.

[x] Stworzenie Samplera i Texture View.

[x] Renderowanie Sprite'ów (Quad)

[x] Zmiana geometrii z trójkąta na prostokąt (Quad) oparty na 4 wierzchołkach i 6 indeksach.

[x] Obsługa współrzędnych UV (teksturowanie).

[x] Shader obsługujący tekstury (sample'owanie koloru).

Phase 2: Wydajność i "Horde Rendering" 🚀

Cel: Nauczenie silnika renderowania 10 000+ obiektów bez spadku FPS (kluczowe dla gatunku Survivor).

[x] GPU Instancing

[x] Przebudowa potoku renderowania: jeden model (Quad) rysowany N razy.

[x] Stworzenie bufora instancji (Instance Buffer) przechowującego: Position, Scale, Rotation, TextureIndex.

[x] System Kamery

[x] Płynne podążanie kamery za graczem.

[x] Przesyłanie macierzy View/Projection do shaderów przez Uniform Buffer.

[x] Texture Atlas

[x] Obsługa atlasów tekstur (wiele sprite'ów w jednym pliku obrazu).

[x] Obliczanie współrzędnych UV dla konkretnych klatek animacji w atlasie.

Phase 3: Architektura Gry (Gameplay Core) 🎮

Cel: Stworzenie struktur danych do zarządzania logiką gry.

[x] Delta Time & Game Loop

[x] Implementacja stałego lub zmiennego kroku czasowego (niezależność fizyki od FPS).

[x] Prosty System Entity (ECS-lite)

[x] Struktura Entity (Player, Enemy, Projectile).

[x] Zarządzanie listą aktywnych obiektów.

[x] Kolizje (AABB/Circle)

[x] Implementacja prostych kolizji prostokąt-prostokąt (Axis-Aligned Bounding Box).

[x] System oddzielania wrogów od siebie (Spatial Grid Separation).

[x] Sterowanie Graczem

[x] Płynne poruszanie postacią.

[x] Obsługa animacji (zmiana klatek w czasie).

Phase 4: Mechaniki "Survivor" ⚔️

Cel: Implementacja reguł gry właściwej.

[x] System Spawnowania Wrogów

[x] Algorytm spawnowania wrogów poza krawędzią ekranu.

[x] Skalowanie trudności (fale przeciwników).

[] System Broni

[x] Logika "Auto-fire" (najbliższy wróg).

[x] Obsługa pocisków (ruch, kolizja, znikanie).

[] Statystyki i Rozwój

[x] System HP (Gracz i Wrogowie).

[x] Dropienie "kryształów XP" po śmierci wroga.

[x] Level Up i proste menu wyboru ulepszenia.

Phase 5: UI i Audio 🔊 (Completed)

Cel: Interfejs użytkownika i udźwiękowienie.

[x] System Audio

[x] Integracja miniaudio.

[x] Odtwarzanie SFX przy ataku/śmierci/zbieraniu/level up.

[x] Text Rendering (Bitmap Fonts)

[x] Ładowanie fontu jako tekstury.

[x] Renderowanie licznika czasu, poziomu i licznika zabójstw.

[x] UI Overlay

[x] Pasek zdrowia (Health Bar) nad głową gracza.

[x] Pasek XP.

[x] Ekran "Game Over".

[x] Ekran "Level Up" z wyborem ulepszeń.

Phase 6: Polish & Build 📦

Cel: Szlifowanie i przygotowanie wersji dystrybucyjnej.

[ ] Juice (Soczystość)

[ ] Screen Shake przy obrażeniach.

[ ] Prosty system cząsteczek (Particles) przy śmierci wrogów.

[ ] Flash effect (błysk) przy trafieniu.

[ ] Dystrybucja

[ ] Konfiguracja CMake do kopiowania zasobów (assets) do folderu wyjściowego.

[ ] Budowanie wersji Release (.exe / .app).