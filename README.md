# Fractal Stream Renderer

A real-time fractal renderer and live-streaming tool built with OpenGL, FFmpeg, and Dear ImGui.

Stream generative fractal visuals — MIDI-reactive, color-animated, formula-morphing — to YouTube, Twitch, Restream, and anywhere else simultaneously, while recording locally at 4K or 8K.

**Free and open-source under the GPL v3.**

---

## What it does

- **22 iteration formulas** — Mandelbrot, Julia, Burning Ship, Newton, Phoenix, Tricorn, sin/cos/tan/exp/sinh/cosh, Celtic, Magnet I, Manowar, Time-spiral, Polar warp, and more — crossfadeable in real time between a Formula A and Formula B slot
- **MIDI-reactive color synthesis** — hue, saturation, and luminance flash on note-on velocity; RGB mode with independent per-channel oscillators; glitch-coupled color flash
- **Euclidean geometry coupling** — SDF orbit-trap coloring and domain warp fold circle/polygon/star/grid geometry into the fractal structure
- **3-D fractals** — Mandelbulb, Mandelbox, and Quaternion Julia rendered via ray-marching
- **Live video texture** — plays any file FFmpeg can decode as the fractal's color source, mapped via escape-time UV
- **Live camera input** — webcam, iPhone Continuity Camera, and screen/window capture (including capturing a Milkdrop / Butterchurn visualizer running on the same monitor)
- **Multi-destination RTMP streaming** — push to YouTube + Twitch + Restream simultaneously via a single interface
- **4K / 8K local recording** — timestamped `.mp4` files, hardware-accelerated encoding (NVENC, VideoToolbox, or libx264/libx265 fallback), YouTube-safe bitrate calculator
- **MIDI I/O** — map any CC or note to any parameter; built-in generative MIDI sequencer; Surge XT integration
- **Glitch engine** — randomised Julia jumps, formula flashes, zoom punches, blend scatter, MIDI output on glitch
- **FFT spectral chain** — gate, frequency shift, smear, phase scramble, harmonic boost, applied to stream and/or recording audio
- **Presets** — save and load any combination of settings; presets apply instantly during live streaming without interrupting the stream
- **Formula auto-cycle** — randomly step Formula A and/or Formula B on a configurable timer for unattended evolving streams

---

## Screenshots / Demo

_Drop a GIF or screenshot here after your first stream._

---

## Build

### Dependencies

| Library | Purpose | Install (Ubuntu/Debian) |
|---------|---------|------------------------|
| GLFW 3 | Window + OpenGL context | `apt install libglfw3-dev` |
| GLEW | OpenGL extension loader | `apt install libglew-dev` |
| GLM | Math (header-only) | git submodule |
| FFmpeg (libav*) | Video decode + RTMP encode | `apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev libavdevice-dev` |
| Dear ImGui | UI panels | git submodule |
| RtMidi | MIDI I/O | `apt install librtmidi-dev` or git submodule |

macOS: install FFmpeg, GLFW, and RtMidi via Homebrew. OpenGL, VideoToolbox, CoreGraphics, CoreMIDI, and CoreAudio come with Xcode.

### Build steps

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/m8w/x.git
cd x

# Ubuntu/Debian deps
sudo apt install cmake libglfw3-dev libglew-dev \
     libavcodec-dev libavformat-dev libavutil-dev \
     libswscale-dev libswresample-dev libavdevice-dev \
     librtmidi-dev

# Configure and build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run
./build/fractal_stream
```

Optional — for screen capture on Linux:
```bash
sudo apt install wmctrl xvfb   # window list + virtual display
```

---

## Quick start

1. **Run** `./build/fractal_stream`
2. **Fractal panel** — pick Formula A and B, drag the A↔B blend slider to crossfade
3. **Video panel** — Browse for a video file (MP4, MKV, etc.) or open a camera/screen
4. **Stream panel** — paste your RTMP URL + stream key, click **Start**
5. **Color Synth panel** — enable it and plug in a MIDI controller for reactive color
6. **Presets panel** — save your setup; load any preset live without dropping the stream

---

## Screen / window capture (single monitor)

**macOS:** Video panel → App Windows → Refresh → select your app → **Capture Window**.  
Uses `CGWindowListCreateImage` — reads the window's layer buffer directly, so it works even when the fractal renderer is in front.  
*(Requires System Settings → Privacy → Screen Recording → allow your terminal.)*

**Linux:** Video panel → Launch Virtual Display :99 → open a terminal → `DISPLAY=:99 ./butterchurn` → Refresh Monitors → select `:99` → **Capture Screen**.

---

## Formula reference

| ID | Name | Recurrence |
|----|------|-----------|
| 0 | Mandelbrot / Julia | z² + c |
| 1 | Sinus | sin(z) + c |
| 2 | Exponential | exp(z) + c |
| 3 | Cosine | cos(z) + c |
| 4 | Sinh | sinh(z) + c |
| 5 | Cosh | cosh(z) + c |
| 6 | Burning Ship | (\|Re z\| + i\|Im z\|)² + c |
| 7 | Tricorn | conj(z)² + c |
| 8 | Newton z³−1 | z − (z³−1)/(3z²) |
| 9 | Phoenix | z² + Re(c) + Im(c)·z_{n-1} |
| 10 | Power | z^n + c |
| 11 | Tangent | tan(z) + c |
| 12 | z·exp(z) | z·e^z + c |
| 13 | Celtic | (\|Re(z²)\|, Im(z²)) + c |
| 14 | Magnet I | ((z²+c−1)/(2z+c−2))² |
| 15 | z^z | z^z + c |
| 16 | Manowar | z² + z_{n-1} + c |
| 17 | Perpendicular Burning Ship | (Re z, \|Im z\|)² + c |
| 18 | Time-spiral | z²·e^(i·param·t) + c |
| 19 | Cubic+linear | z³ + z + c |
| 20 | Cosh-conjugate | cosh(conj(z)) + c |
| 21 | Polar→Cartesian warp | polar coord remap before squaring |

Formula A and B are selected independently and crossfaded with the **A↔B blend** slider. Enable **Random cycle A/B** to auto-rotate formulas on a timer.

---

## MIDI mapping

Any slider, checkbox, or combo in the UI can be bound to a MIDI CC or note:

1. MIDI panel → **Learn** → move the target slider → wiggle your controller knob
2. The mapping appears in the table; drag Min/Max to set the range
3. Save a preset to persist your mappings

---

## Streaming to Restream / YouTube / Twitch

Stream panel → add a destination row → paste RTMP URL + key → **Start Stream**.

Common ingest URLs:
- YouTube: `rtmp://a.rtmp.youtube.com/live2/<key>`
- Twitch: `rtmp://live.twitch.tv/app/<key>`
- Restream: `rtmp://live.restream.io/live/<key>`

Restream lets you forward one stream to 30+ platforms simultaneously.

---

## Recording locally (4K / 8K)

Recording panel → set output path (Browse opens a save dialog) → choose 4K or 8K, bitrate, and fps → **Start Recording**.

The YouTube safe-bitrate calculator shows the maximum bitrate that keeps a target-length recording under YouTube's 256 GB file limit.

Default: `~/fractal_YYYYMMDD_HHMMSS.mp4`

---

## License

GPL v3 — see [LICENSE](LICENSE).  
Free to use, share, and modify. Derivative works must also be GPL v3.
