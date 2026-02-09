cmake_minimum_required(VERSION 3.10)
project(WarpEngine)

# Serce projektu (konfiguracja)
# Tu konfigurujemy projekt i dołączamy zależności (wgpu, glfw, itp.)

set(CMAKE_CXX_STANDARD 17)

# add_subdirectory(external/wgpu) # Przykład
# add_subdirectory(external/glfw) # Przykład

add_executable(WarpEngine src/main.cpp)

# target_link_libraries(WarpEngine PRIVATE wgpu glfw) # Przykład
