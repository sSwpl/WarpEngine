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

[ ] GPU Instancing

[ ] Przebudowa potoku renderowania: jeden model (Quad) rysowany N razy.

[ ] Stworzenie bufora instancji (Instance Buffer) przechowującego: Position, Scale, Rotation, TextureIndex.

[ ] System Kamery

[ ] Płynne podążanie kamery za graczem.

[ ] Przesyłanie macierzy View/Projection do shaderów przez Uniform Buffer.

[ ] Texture Atlas

[ ] Obsługa atlasów tekstur (wiele sprite'ów w jednym pliku obrazu).

[ ] Obliczanie współrzędnych UV dla konkretnych klatek animacji w atlasie.

Phase 3: Architektura Gry (Gameplay Core) 🎮

Cel: Stworzenie struktur danych do zarządzania logiką gry.

[ ] Delta Time & Game Loop

[ ] Implementacja stałego lub zmiennego kroku czasowego (niezależność fizyki od FPS).

[ ] Prosty System Entity (ECS-lite)

[ ] Struktura Entity (Player, Enemy, Projectile).

[ ] Zarządzanie listą aktywnych obiektów.

[ ] Kolizje (AABB)

[ ] Implementacja prostych kolizji prostokąt-prostokąt (Axis-Aligned Bounding Box).

[ ] System oddzielania wrogów od siebie (żeby nie wchodzili w jeden punkt).

[ ] Sterowanie Graczem

[ ] Płynne poruszanie postacią.

[ ] Obsługa animacji (zmiana klatek w czasie).

Phase 4: Mechaniki "Survivor" ⚔️

Cel: Implementacja reguł gry właściwej.

[ ] System Spawnowania Wrogów

[ ] Algorytm spawnowania wrogów poza krawędzią ekranu.

[ ] Skalowanie trudności (fale przeciwników).

[ ] System Broni

[ ] Logika "Auto-fire" (najbliższy wróg).

[ ] Obsługa pocisków (ruch, kolizja, znikanie).

[ ] Statystyki i Rozwój

[ ] System HP (Gracz i Wrogowie).

[ ] Dropienie "kryształów XP" po śmierci wroga.

[ ] Level Up i proste menu wyboru ulepszenia.

Phase 5: UI i Audio 🔊

Cel: Interfejs użytkownika i udźwiękowienie.

[ ] System Audio

[ ] Integracja miniaudio lub Soloud.

[ ] Odtwarzanie SFX przy ataku/śmierci.

[ ] Text Rendering (Bitmap Fonts)

[ ] Ładowanie fontu jako tekstury.

[ ] Renderowanie licznika czasu, poziomu i licznika zabójstw.

[ ] UI Overlay

[ ] Pasek zdrowia (Health Bar) nad głową gracza/wrogów.

[ ] Ekran "Game Over".

Phase 6: Polish & Build 📦

Cel: Szlifowanie i przygotowanie wersji dystrybucyjnej.

[ ] Juice (Soczystość)

[ ] Screen Shake przy obrażeniach.

[ ] Prosty system cząsteczek (Particles) przy śmierci wrogów.

[ ] Flash effect (błysk) przy trafieniu.

[ ] Dystrybucja

[ ] Konfiguracja CMake do kopiowania zasobów (assets) do folderu wyjściowego.

[ ] Budowanie wersji Release (.exe / .app).