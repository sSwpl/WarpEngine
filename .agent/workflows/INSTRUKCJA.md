# 🚀 Instrukcja — Jak odpalać projekty C++ (CMake + MSVC na Windows)

> **Dla osób przychodzących z Pythona:**  
> W Pythonie piszesz `python main.py` i działa. W C++ jest dodatkowy krok — **kompilacja**.  
> Najpierw kompilator zamienia Twój kod `.cpp` na plik `.exe`, a potem uruchamiasz ten `.exe`.

---

## 📁 Struktura projektu

```
WarpEngine/
├── CMakeLists.txt      ← "przepis" na budowanie (jak pyproject.toml w Pythonie)
├── src/
│   └── main.cpp        ← Twój kod źródłowy
├── build/              ← folder z wynikami kompilacji (tworzony automatycznie)
│   └── WarpEngine.exe  ← skompilowany program
└── INSTRUKCJA.md       ← ten plik
```

---

## 🆕 Nowy projekt — pierwsza kompilacja

Kiedy tworzysz nowy projekt C++ (albo klonujesz go po raz pierwszy), musisz wykonać **3 kroki**:

### Krok 1: Otwórz terminal (PowerShell) w folderze projektu

### Krok 2: Skonfiguruj i zbuduj projekt

Wklej tę komendę w PowerShell:

```powershell
cmd /c "`"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat`" x64 && cmake -S . -B build -G Ninja && cmake --build build"
```

**Co ta komenda robi (krok po kroku):**

| Fragment | Co robi |
|----------|---------|
| `vcvarsall.bat x64` | Ustawia zmienne środowiskowe kompilatora MSVC (jak `source venv/bin/activate` w Pythonie) |
| `cmake -S . -B build -G Ninja` | **Konfiguracja** — CMake czyta `CMakeLists.txt` i generuje pliki budowania w folderze `build/` |
| `cmake --build build` | **Kompilacja** — zamienia `.cpp` na `.exe` |

### Krok 3: Uruchom program

```powershell
.\build\WarpEngine.exe
```

Powinieneś zobaczyć:
```
Warp Engine Initialized
```

---

## 🔄 Już skonfigurowany projekt — ponowna kompilacja po zmianach

Gdy **zmieniłeś tylko kod** w plikach `.cpp` / `.h` (nie ruszałeś `CMakeLists.txt`), wystarczy:

### Krok 1: Przebuduj

```powershell
cmd /c "`"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat`" x64 && cmake --build build"
```

> ⚡ Uwaga: nie trzeba ponownie robić `cmake -S . -B build` — konfiguracja jest już zapisana w folderze `build/`.  
> CMake sam wykryje, które pliki się zmieniły i skompiluje tylko te zmienione (szybciej!).

### Krok 2: Uruchom

```powershell
.\build\WarpEngine.exe
```

---

## 🛠️ Kiedy trzeba rekonfigurować (pełna komenda)?

Pełną komendę z `cmake -S . -B build` musisz użyć, gdy:

- ✏️ **Zmieniłeś `CMakeLists.txt`** (np. dodałeś nowe pliki źródłowe, biblioteki)
- 🗑️ **Usunąłeś folder `build/`**
- 📦 **Dodałeś nową zależność** (np. GLFW, WebGPU)
- 🆕 **Klonujesz projekt na nowy komputer**

---

## 📝 Dodawanie nowych plików `.cpp` do projektu

Gdy tworzysz nowy plik (np. `src/engine.cpp`), musisz go **dodać do `CMakeLists.txt`**:

```cmake
# Było:
add_executable(WarpEngine src/main.cpp)

# Zamień na:
add_executable(WarpEngine
    src/main.cpp
    src/engine.cpp
)
```

Potem uruchom **pełną komendę** (z krokami konfiguracji).

---

## ❌ Typowe błędy i rozwiązania

| Problem | Rozwiązanie |
|---------|-------------|
| `'cmake' is not recognized` | CMake nie jest w PATH — zainstaluj go ponownie lub dodaj do PATH |
| `ninja: error: loading 'build.ninja'` | Usuń folder `build/` i uruchom pełną komendę |
| `error C2065: undeclared identifier` | Literówka w kodzie — sprawdź nazwy zmiennych |
| `LNK2019: unresolved external symbol` | Zapomniałeś dodać plik `.cpp` do `CMakeLists.txt` |
| `fatal error C1083: Cannot open include file` | Brakuje biblioteki lub złe `#include` |

---

## 🔑 Szybka ściągawka

```
┌─────────────────────────────────────────────────────────────────┐
│  NOWY PROJEKT (pierwszy raz):                                   │
│                                                                 │
│  cmd /c "`"...\vcvarsall.bat`" x64 &&                          │
│         cmake -S . -B build -G Ninja &&                         │
│         cmake --build build"                                    │
│  .\build\WarpEngine.exe                                         │
├─────────────────────────────────────────────────────────────────┤
│  PO ZMIANACH W KODZIE (przebudowa):                             │
│                                                                 │
│  cmd /c "`"...\vcvarsall.bat`" x64 &&                          │
│         cmake --build build"                                    │
│  .\build\WarpEngine.exe                                         │
├─────────────────────────────────────────────────────────────────┤
│  PO ZMIANIE CMakeLists.txt (rekonfiguracja):                    │
│                                                                 │
│  (użyj pełnej komendy jak dla nowego projektu)                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 🐍 Porównanie z Pythonem

| Koncept | Python | C++ |
|---------|--------|-----|
| Uruchamianie | `python main.py` | Kompiluj → `.\build\WarpEngine.exe` |
| Build system | `pip` / `pyproject.toml` | **CMake** / `CMakeLists.txt` |
| Dodawanie plików | `import module` i działa | Dodaj do `CMakeLists.txt` + `#include` |
| Instalacja zależności | `pip install X` | Dodaj do `CMakeLists.txt` + pobierz źródła |
| Typy zmiennych | Dynamiczne (`x = 5`) | Statyczne (`int x = 5;`) |
| Pamięć | Automatyczna (GC) | Ręczna / smart pointery |
