cmake_minimum_required(VERSION 3.20)
project(WarpEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. GLFW (Windowing) ---
# Automatically downloads and builds GLFW
include(FetchContent)
FetchContent_Declare(glfw URL https://github.com/glfw/glfw/releases/download/3.3.8/glfw-3.3.8.zip)
FetchContent_MakeAvailable(glfw)

# --- 2. WebGPU (Graphics) ---
# Expects wgpu-native in external/wgpu
set(WGPU_ROOT "${CMAKE_SOURCE_DIR}/external/wgpu")

# --- 3. Executable ---
add_executable(WarpEngine src/main.cpp)

if(EXISTS "${WGPU_ROOT}")
    # Include Header
    # wgpu-native usually has headers in 'include/webgpu' or just 'include' depending on how you unpack
    # We will assume include/webgpu/webgpu.h structure or include/webgpu.h
    target_include_directories(WarpEngine PUBLIC "${WGPU_ROOT}/include")
    
    # Find Library
    find_library(WGPU_LIB 
        NAMES wgpu_native webgpu
        PATHS "${WGPU_ROOT}/lib" 
        NO_DEFAULT_PATH
    )
    
    if(WGPU_LIB)
        message(STATUS "Found WebGPU library: ${WGPU_LIB}")
    else()
        message(WARNING "WebGPU library not found in ${WGPU_ROOT}/lib setup. Run download_libs.sh!")
    endif()
else()
    message(WARNING "external/wgpu directory missing. Please run download_libs.sh")
endif()

# Link Libraries
target_link_libraries(WarpEngine PUBLIC glfw)

if(WGPU_LIB)
    target_link_libraries(WarpEngine PUBLIC ${WGPU_LIB})
    target_include_directories(WarpEngine PUBLIC "${WGPU_ROOT}/include")

    # Copy shared library to output directory so the app can find it at runtime
    add_custom_command(TARGET WarpEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${WGPU_LIB}
        $<TARGET_FILE_DIR:WarpEngine>
    )
endif()

# macOS specific frameworks (often needed for windowing/graphics)
if(APPLE)
    target_link_libraries(WarpEngine PUBLIC "-framework Cocoa" "-framework CoreVideo" "-framework IOKit" "-framework QuartzCore")
endif()