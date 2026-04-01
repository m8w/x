# Fractal Stream 

A real-time generative video synthesis engine and live streaming tool for macOS (Linux/Windows in progress). It merges a GPU-accelerated fractal renderer and a MilkDrop audio-reactive visualizer into a single unified pipeline that encodes and pushes the result to any number of RTMP destinations simultaneously.

---

## What it does

The app renders a fullscreen fractal on the GPU every frame, blends it with a decoded local video file mapped onto the fractal surface using escape-time UV coordinates, layers on MilkDrop-style audio-reactive visuals, and streams the composited output to Restream (or any RTMP endpoint) in real time — all while exposing every parameter through a live Dear ImGui control panel.

---

## Fractal engine

Five fractal types blend simultaneously with independent weight sliders:

- **Mandelbrot / Julia** — classic escape-time with configurable Julia C constant
- **Mandelbulb** — 3D ray-marched using the Inigo Quilez distance estimator
- **Mandelbox** — IFS fold-and-scale variant with configurable scale and box fold limit
- **Quaternion Julia** — 4D Julia set projected to 3D via ray marching
- **Euclidean geometry** — signed-distance fields (circle, polygon, star, grid) coupled into the fractal orbit as orbit-trap coloring and domain warp

Blend weights are normalised in-shader so they always sum to 1, allowing smooth morphs between any combination.

**22 iteration formulas** are available and any two can be cross-faded with a blend slider:

| # | Formula |
|---|---------|
| 0 | z² + c (classic Mandelbrot/Julia) |
| 1–5 | sin, exp, cos, sinh, cosh |
| 6 | Burning Ship (absolute-value fold) |
| 7 | Tricorn / Mandelbar (conjugate) |
| 8 | Newton z³−1 (convergence coloring) |
| 9 | Phoenix (two-step memory recurrence) |
| 10 | zⁿ + c (arbitrary real power) |
| 11–21 | tan, z·exp(z), Celtic, Magnet I, zᶻ, Manowar, Perp Burning Ship, Time-spiral, z³+z+c, cosh(conj(z)), Polar→Cart warp |

The GLSL complex math library covers arithmetic, exp/log, arbitrary real and complex power via polar form, full trig and hyperbolic families — matching the UltraFractal / Fractal Explorer function set.

---

## Video input

FFmpeg decodes any local video file (MP4, MKV, MOV, etc.) frame-by-frame. Each decoded frame is converted from YUV to RGB via libswscale and uploaded to a GL texture. The shader samples this texture using fractal-derived UV coordinates computed from the escape-time value — the video image is literally painted onto the fractal surface, with different parts of the video appearing at different iteration depths.

---

## MilkDrop visualizer

A complete C++/OpenGL port of the classic Winamp visualizer engine. Runs as a 5-pass GPU pipeline every frame:

1. **Warp pass** — samples the previous frame through a zoom/rotation/translation/warp distortion computed by the preset's per-frame equations, with decay and gamma correction. This feedback loop gives MilkDrop its flowing, morphing motion.
2. **Wave pass** — renders the live audio waveform as a VBO line strip or point cloud, with per-vertex color derived from the spectrum and beat strength.
3. **Shape pass** — renders up to 4 preset-defined shapes as CPU-generated triangle fans with radial gradients, supporting additive and alpha blending.
4. **Composite pass** — merges warp texture + wave + shapes + optional fractal overlay, applies gamma and vignette.
5. **Blend pass** — when transitioning between presets, crossfades using one of 10 transition modes: hard cut, crossfade, zoom in/out, wipe left/right, spin CW/CCW, pixelate, dissolve.

**Preset expressions** are evaluated using the `projectm-eval` library (float precision), implementing the full MilkDrop expression language — the per-frame equations that drive zoom, rotation, warp, decay, gamma, and 32 q-variables across thousands of community `.milk` files.

**Audio analysis** runs on a dedicated AVAudioEngine tap (macOS). A 1024-sample Hann-windowed vDSP FFT produces 256 log-reduced spectrum bins. Bass (20–200 Hz), mid (200–2000 Hz), and treble (2–20 kHz) RMS bands are smoothed per-frame and fed into both the MilkDrop evaluator and the beat detector.

**Beat detection** uses an adaptive threshold from a 60-frame rolling bass history. Four hardcut modes (Bass, Treble, Bass AND Treble, Bass OR Treble) trigger instant preset switches. BPM is estimated from the inter-beat interval of the last 8 onsets.

---

## RTMP streaming

Multiple destinations can be added simultaneously — each with its own URL and stream key — enabling fan-out to Restream, Twitch, YouTube, and any RTMP endpoint in a single pass. Per-destination enable/disable lets you mute individual targets without stopping the encode.

The stream source is selectable live: either the fractal FBO or the MilkDrop composite output, switchable without restarting the stream.

Pixel readback uses double-buffered PBOs for async GPU→CPU transfer, avoiding pipeline stalls. Resolution: 720p / 1080p / 1440p / 4K. Bitrate: 1–40 Mbps.

---

## MIDI

- **MIDI input** — CC/note messages map to any fractal parameter via a learn-mode mapper. All mappings persist across sessions.
- **MIDI generator** — produces notes, chords, and program changes on a configurable BPM grid with scale quantisation, velocity humanisation, rest probability, and chord size. Output goes to any connected MIDI port.
- **MIDI thru** — optionally forwards incoming MIDI to the output port.
- **Glitch engine** — stochastically fires MIDI events (velocity spikes, pitch scrambles, ghost notes) and fractal parameter jumps (Julia jump, formula flash, zoom punch, blend scatter) at configurable rate and duration.

---

## HTTP remote

A built-in HTTP server on port 7777 exposes fractal parameters as a JSON REST API, letting you control the engine from any browser or phone on the local network.

---

## Session persistence

All state — fractal parameters, blend weights, stream destinations, MIDI mappings, MilkDrop preset selection, beat detector thresholds, audio device, auto-advance settings, and fractal overlay blend — is saved to an INI file on quit and restored on next launch.

---

## Architecture

```
AVAudioEngine tap → FFT → AudioData → BeatDetector
                                    ↓
FFmpeg decode → GL texture          MilkDropGLRenderer (5-pass FBO pipeline)
                    ↓                        ↓
              Renderer (fractal GLSL) → composite → RTMP encode (libx264)
                    ↓
              ImGui panels (EquationEditor) ← MIDI input / HTTP remote
```

Everything runs on a single thread with the OpenGL context. Audio capture runs on AVAudioEngine's internal thread and deposits frames into a ring buffer that the render loop polls non-blocking each frame.

---

## Build

```bash
# clone with submodules
git clone --recurse-submodules <repo>

# install system deps (macOS)
brew install cmake glfw ffmpeg

# build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# run
./build/fractal_stream /path/to/video.mp4
```

---

## Dependencies

| Library | Purpose |
|---------|---------|
| GLFW 3 | Window + OpenGL context |
| GLM | Vector/matrix math (header-only) |
| FFmpeg (libav*) | Video decode + RTMP encode |
| Dear ImGui | Live parameter editor UI |
| RtMidi | MIDI input/output |
| projectm-eval | MilkDrop expression language evaluator |
| OpenGL 4.1 | GPU shader execution |
