cmake_minimum_required(VERSION 3.18)
project(fractal_stream VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Third-party submodules ────────────────────────────────────────────────────
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
set(GLM_DIR   ${CMAKE_SOURCE_DIR}/third_party/glm)

# ── System packages ───────────────────────────────────────────────────────────
find_package(OpenGL REQUIRED)
find_package(GLEW   REQUIRED)
find_package(glfw3  REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavcodec libavformat libavutil libswscale)

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
)

add_executable(fractal_stream ${SOURCES} ${IMGUI_SOURCES})

target_include_directories(fractal_stream PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GLM_DIR}
    ${FFMPEG_INCLUDE_DIRS}
)

target_compile_definitions(fractal_stream PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_GLEW
    SHADERS_DIR="${CMAKE_SOURCE_DIR}/shaders"
)

target_link_libraries(fractal_stream PRIVATE
    OpenGL::GL
    GLEW::GLEW
    glfw
    ${FFMPEG_LIBRARIES}
)

target_compile_options(fractal_stream PRIVATE -O2)
