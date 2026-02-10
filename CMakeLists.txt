cmake_minimum_required(VERSION 3.20)
if(APPLE)
    project(WarpEngine LANGUAGES CXX OBJCXX)
else()
    project(WarpEngine LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. GLFW (Windowing) ---
# If pre-built GLFW exists in external/glfw (from download_libs.ps1), use it.
# Otherwise, fallback to FetchContent (auto-download + build from source — works on Mac/Linux).
set(GLFW_ROOT "${CMAKE_SOURCE_DIR}/external/glfw")

if(EXISTS "${GLFW_ROOT}")
    message(STATUS "Using pre-built GLFW from ${GLFW_ROOT}")
    find_library(GLFW_LIB
        NAMES glfw3
        PATHS "${GLFW_ROOT}/lib"
        NO_DEFAULT_PATH
    )
    if(GLFW_LIB)
        add_library(glfw STATIC IMPORTED)
        set_target_properties(glfw PROPERTIES
            IMPORTED_LOCATION "${GLFW_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${GLFW_ROOT}/include"
        )
    else()
        message(FATAL_ERROR "GLFW lib not found in ${GLFW_ROOT}/lib — delete external/glfw and re-run download_libs.ps1")
    endif()
else()
    message(STATUS "external/glfw not found — downloading GLFW via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(glfw URL https://github.com/glfw/glfw/releases/download/3.3.8/glfw-3.3.8.zip)
    FetchContent_MakeAvailable(glfw)
endif()

# --- 2. WebGPU (Graphics) ---
# Expects wgpu-native in external/wgpu
set(WGPU_ROOT "${CMAKE_SOURCE_DIR}/external/wgpu")

# --- 3. Executable ---
set(WARP_SOURCES
    src/main.cpp
    src/wgpu_surface.cpp
)

if(APPLE)
    list(APPEND WARP_SOURCES src/wgpu_surface_macos.mm)
endif()

add_executable(WarpEngine ${WARP_SOURCES})

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

    # Copy the DLL to output directory so the app can find it at runtime
    if(WIN32)
        add_custom_command(TARGET WarpEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WGPU_ROOT}/lib/wgpu_native.dll"
            $<TARGET_FILE_DIR:WarpEngine>
        )
    elseif(APPLE)
        add_custom_command(TARGET WarpEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WGPU_ROOT}/lib/libwgpu_native.dylib"
            $<TARGET_FILE_DIR:WarpEngine>
        )
    else()
        add_custom_command(TARGET WarpEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WGPU_ROOT}/lib/libwgpu_native.so"
            $<TARGET_FILE_DIR:WarpEngine>
        )
    endif()
endif()

# macOS specific frameworks (often needed for windowing/graphics)
if(APPLE)
    target_link_libraries(WarpEngine PUBLIC "-framework Cocoa" "-framework CoreVideo" "-framework IOKit" "-framework QuartzCore" "-framework Metal")
endif()