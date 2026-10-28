cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

project(integrate-sdl3-image-webp-wasm CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# Paths
# -----------------------------------------------------------------------------
set(SDL3_DIR       "C:/libs/sdl3-3.4.18-wasm")
set(SDL3_image_DIR "C:/libs/sdl3_image-3.4.8-webp-wasm")
set(WEBP_DIR       "C:/libs/webp-1.6.0-wasm")

# -----------------------------------------------------------------------------
# Executable Target (Emscripten generates .js and .wasm)
# -----------------------------------------------------------------------------
add_executable(app
    src/main.cpp
)

# Set output name to app.js
set_target_properties(app PROPERTIES
    OUTPUT_NAME "app"
    SUFFIX ".js"
)

# -----------------------------------------------------------------------------
# Include Directories
# -----------------------------------------------------------------------------
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SDL3_DIR}/include
    ${SDL3_image_DIR}/include
    ${WEBP_DIR}/include
)

# -----------------------------------------------------------------------------
# Link Static Libraries
# -----------------------------------------------------------------------------
target_link_libraries(app PRIVATE
    ${SDL3_image_DIR}/lib/libSDL3_image.a
    ${SDL3_DIR}/lib/libSDL3.a
    ${WEBP_DIR}/lib/libwebpdemux.a
    ${WEBP_DIR}/lib/libwebpmux.a
    ${WEBP_DIR}/lib/libwebp.a
    ${WEBP_DIR}/lib/libsharpyuv.a
)

# -----------------------------------------------------------------------------
# Emscripten Link Options (WebGL 2.0, Memory Growth, Preloaded Assets)
# -----------------------------------------------------------------------------
target_link_options(app PRIVATE
    "-sUSE_WEBGL2=1"
    "-sFULL_ES3=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/assets@assets"
)
