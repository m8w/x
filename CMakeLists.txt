cmake_minimum_required(VERSION 3.18)

# OBJCXX is only available on macOS (for FilePicker.mm)
if(APPLE)
    project(fractal_stream VERSION 0.1 LANGUAGES CXX OBJCXX)
else()
    project(fractal_stream VERSION 0.1 LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── macOS: Homebrew paths + silence OpenGL deprecation warnings ───────────────
if(APPLE)
    # Homebrew on Apple Silicon installs to /opt/homebrew; Intel Macs use /usr/local
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew /usr/local)
    # Let CMake target the native CPU arch (arm64 on M-series, x86_64 on Intel)
    # Do NOT hard-code arm64 here — that breaks Intel MacBook builds.
    add_compile_definitions(GL_SILENCE_DEPRECATION)
endif()

# ── Windows / MSYS2: prefer static linking for portability ───────────────────
if(WIN32)
    # MSYS2 sets CMAKE_PREFIX_PATH via toolchain; pkg-config finds mingw packages
    add_compile_definitions(
        UNICODE _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# ── Third-party (submodules + bundled) ───────────────────────────────────────
set(IMGUI_DIR       ${CMAKE_SOURCE_DIR}/third_party/imgui)
set(GLM_DIR         ${CMAKE_SOURCE_DIR}/third_party/glm)
set(RTMIDI_DIR      ${CMAKE_SOURCE_DIR}/third_party/rtmidi)
set(PROJECTM_EVAL_DIR ${CMAKE_SOURCE_DIR}/third_party/projectm-eval)

# ── projectm-eval (MilkDrop expression evaluator) ────────────────────────────
# Use float (4) instead of double (8) to match our GPU uniform precision.
set(PROJECTM_EVAL_FLOAT_SIZE "4" CACHE STRING "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BUILD_NS_EEL_SHIM OFF CACHE BOOL "" FORCE)
add_subdirectory(${PROJECTM_EVAL_DIR} ${CMAKE_BINARY_DIR}/projectm-eval EXCLUDE_FROM_ALL)

# ── System packages ───────────────────────────────────────────────────────────
find_package(OpenGL REQUIRED)
find_package(glfw3  REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavcodec libavformat libavutil libswscale libswresample libavdevice)
if(NOT APPLE)
    find_package(GLEW REQUIRED)
endif()

# ── RtMidi: prefer system package, fall back to bundled third_party/rtmidi ───
pkg_check_modules(RTMIDI QUIET rtmidi)
if(NOT RTMIDI_FOUND)
    find_package(RtMidi QUIET)
    if(RtMidi_FOUND)
        set(RTMIDI_LIBRARIES RtMidi::rtmidi)
        set(RTMIDI_FOUND TRUE)
    endif()
endif()

if(RTMIDI_FOUND)
    message(STATUS "RtMidi: using system library")
    set(RTMIDI_EXTRA_SOURCES "")
    set(RTMIDI_EXTRA_INCLUDES ${RTMIDI_INCLUDE_DIRS})
    set(RTMIDI_LINK_LIBS ${RTMIDI_LIBRARIES})
else()
    message(STATUS "RtMidi: using bundled third_party/rtmidi")
    set(RTMIDI_EXTRA_SOURCES ${RTMIDI_DIR}/RtMidi.cpp)
    set(RTMIDI_EXTRA_INCLUDES ${RTMIDI_DIR})
    if(APPLE)
        set(RTMIDI_LINK_LIBS "")   # CoreMIDI added below in framework block
    elseif(WIN32)
        set(RTMIDI_LINK_LIBS winmm)
    else()
        set(RTMIDI_LINK_LIBS asound)   # ALSA on Linux
    endif()
    # Tell RtMidi which backend to compile
    if(APPLE)
        add_compile_definitions(__MACOSX_CORE__)
    elseif(WIN32)
        add_compile_definitions(__WINDOWS_MM__)
    else()
        add_compile_definitions(__LINUX_ALSA__)
    endif()
endif()

# ── GLEW (Linux + Windows, not macOS) ────────────────────────────────────────

# ── ImGui sources ─────────────────────────────────────────────────────────────
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
    src/AppSettings.cpp
    src/renderer/Renderer.cpp
    src/renderer/ShaderProgram.cpp
    src/renderer/VideoTexture.cpp
    src/fractal/FractalEngine.cpp
    src/fractal/BlendController.cpp
    src/fractal/GlitchEngine.cpp
    src/fractal/ColorSynth.cpp
    src/fx/FftChain.cpp
    src/stream/VideoInput.cpp
    src/stream/StreamOutput.cpp
    src/stream/RecordOutput.cpp
    src/ui/EquationEditor.cpp
    src/remote/RemoteControl.cpp
    src/midi/MidiInput.cpp
    src/midi/MidiMapper.cpp
    src/midi/MidiGenerator.cpp
    src/midi/MidiOutput.cpp
)

if(APPLE)
    list(APPEND SOURCES src/ui/FilePicker.mm)
    list(APPEND SOURCES src/audio/AudioCapture_mac.mm)
elseif(WIN32)
    list(APPEND SOURCES src/ui/FilePicker_windows.cpp)
    list(APPEND SOURCES src/audio/AudioCapture_portaudio.cpp)
else()
    list(APPEND SOURCES src/ui/FilePicker_linux.cpp)
    list(APPEND SOURCES src/audio/AudioCapture_portaudio.cpp)
endif()

list(APPEND SOURCES
    src/audio/BeatDetector.cpp
    src/milkdrop/MilkDropPreset.cpp
    src/milkdrop/PresetParser.cpp
    src/milkdrop/PresetManager.cpp
    src/milkdrop/EquationEvaluator.cpp
    src/milkdrop/MilkDropGLRenderer.cpp
)

add_executable(fractal_stream ${SOURCES} ${IMGUI_SOURCES} ${RTMIDI_EXTRA_SOURCES})

target_include_directories(fractal_stream PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GLM_DIR}
    ${FFMPEG_INCLUDE_DIRS}
    ${RTMIDI_EXTRA_INCLUDES}
    ${PROJECTM_EVAL_DIR}
)

target_link_directories(fractal_stream PRIVATE ${FFMPEG_LIBRARY_DIRS})

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

# On Windows, embed the shaders directory path as a resource-relative path so
# the installed binary can find shaders next to the exe.
if(WIN32)
    target_compile_definitions(fractal_stream PRIVATE
        SHADERS_DIR_RELATIVE="shaders"
    )
endif()

# ── Link ──────────────────────────────────────────────────────────────────────
if(APPLE)
    target_link_libraries(fractal_stream PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework CoreMedia"
        "-framework VideoToolbox"
        "-framework CoreMIDI"
        "-framework CoreAudio"
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework UniformTypeIdentifiers"
        "-framework AVFoundation"
        "-framework Accelerate"
        glfw
        ${FFMPEG_LIBRARIES}
        ${RTMIDI_LINK_LIBS}
        projectM_eval
    )
elseif(WIN32)
    target_link_libraries(fractal_stream PRIVATE
        OpenGL::GL
        GLEW::GLEW
        glfw
        ${FFMPEG_LIBRARIES}
        ${RTMIDI_LINK_LIBS}
        ws2_32
        ole32
        oleaut32
        uuid
        strmiids
        winmm
    )
else()
    target_link_libraries(fractal_stream PRIVATE
        OpenGL::GL
        GLEW::GLEW
        glfw
        ${FFMPEG_LIBRARIES}
        ${RTMIDI_LINK_LIBS}
        projectM_eval
        pthread
    )
endif()

target_compile_options(fractal_stream PRIVATE -O2)
