---
description: Pełna konfiguracja + budowanie WarpEngine (nowy projekt lub po zmianach w CMakeLists.txt)
---

// turbo-all

1. Skonfiguruj i zbuduj projekt od nowa:
```powershell
cmd /c "`"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat`" x64 && cmake -S . -B build -G Ninja && cmake --build build"
```
Cwd: c:\Users\pc\PROJEKTY_GROWE\WarpEngine

2. Uruchom program:
```powershell
.\build\WarpEngine.exe
```
Cwd: c:\Users\pc\PROJEKTY_GROWE\WarpEngine
