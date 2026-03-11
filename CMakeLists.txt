cmake_minimum_required(VERSION 3.18)
project(fractal_stream VERSION 0.1 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Apple Silicon / Homebrew paths ────────────────────────────────────────────
if(APPLE)
    # Homebrew on Apple Silicon lives at /opt/homebrew
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew)
    set(CMAKE_OSX_ARCHITECTURES arm64)
    # Silence OpenGL deprecation warnings (Apple deprecated GL but it still works)
    add_compile_definitions(GL_SILENCE_DEPRECATION)
endif()

# ── Third-party submodules ────────────────────────────────────────────────────
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
set(GLM_DIR   ${CMAKE_SOURCE_DIR}/third_party/glm)

# ── System packages ───────────────────────────────────────────────────────────
find_package(OpenGL REQUIRED)
find_package(glfw3  REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavcodec libavformat libavutil libswscale)

# GLEW: not needed on macOS — use the built-in GL loader instead
if(NOT APPLE)
    find_package(GLEW REQUIRED)
endif()

# ── ImGui sources (compiled directly) ────────────────────────────────────────
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# ── Project sources ───────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/renderer/Renderer.cpp
    src/renderer/ShaderProgram.cpp
    src/renderer/VideoTexture.cpp
    src/fractal/FractalEngine.cpp
    src/fractal/BlendController.cpp
    src/stream/VideoInput.cpp
    src/stream/StreamOutput.cpp
    src/ui/EquationEditor.cpp
    src/ui/FilePicker_linux.cpp  # no-op on macOS via #ifndef __APPLE__
    src/remote/RemoteControl.cpp
)

# macOS: native NSOpenPanel file picker (Objective-C++)
if(APPLE)
    list(APPEND SOURCES src/ui/FilePicker.mm)
endif()

add_executable(fractal_stream ${SOURCES} ${IMGUI_SOURCES})

target_include_directories(fractal_stream PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GLM_DIR}
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_directories(fractal_stream PRIVATE ${FFMPEG_LIBRARY_DIRS})

# On macOS: no loader flag — ImGui uses its bundled imgui_impl_opengl3_loader.h
# On Linux: tell ImGui to use GLEW
if(APPLE)
    target_compile_definitions(fractal_stream PRIVATE
        SHADERS_DIR="${CMAKE_SOURCE_DIR}/shaders"
    )
else()
    target_compile_definitions(fractal_stream PRIVATE
        IMGUI_IMPL_OPENGL_LOADER_GLEW
        SHADERS_DIR="${CMAKE_SOURCE_DIR}/shaders"
    )
endif()

# ── Link ──────────────────────────────────────────────────────────────────────
if(APPLE)
    target_link_libraries(fractal_stream PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework UniformTypeIdentifiers"
        glfw
        ${FFMPEG_LIBRARIES}
    )
else()
    target_link_libraries(fractal_stream PRIVATE
        OpenGL::GL
        GLEW::GLEW
        glfw
        ${FFMPEG_LIBRARIES}
        pthread
    )
endif()

target_compile_options(fractal_stream PRIVATE -O2)
