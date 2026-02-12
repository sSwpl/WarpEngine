---
description: Buduj i uruchom projekt WarpEngine (po zmianach w kodzie)
---

// turbo-all

1. Przebuduj projekt (kompilacja zmienionych plików):
```powershell
cmd /c "`"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat`" x64 && cmake --build build"
```
Cwd: c:\Users\pc\PROJEKTY_GROWE\WarpEngine

2. Uruchom program:
```powershell
.\build\WarpEngine.exe
```
Cwd: c:\Users\pc\PROJEKTY_GROWE\WarpEngine
