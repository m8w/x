# CLAUDE.md — Fractal Video Stream Renderer

## Project Overview

Real-time fractal renderer that:
- Decodes **local video files** as live input texture
- Blends **Mandelbrot, Julia, Mandelbulb, Mandelbox, Quaternion Julia** with **Euclidean geometry** via a live equation editor
- Supports **11 iteration formulas** (classic z²+c, Burning Ship, Newton, Phoenix, trig/hyperbolic, arbitrary power) inspired by the Fractal Explorer / UltraFractal function set
- Couples SDF geometry into the fractal orbit via **orbit-trap coloring** and **domain warp**
- Maps the video stream onto the fractal surface using escape-time UV mapping
- Encodes and pushes the rendered output to **Restream** (and multiple simultaneous destinations) via RTMP/RTMPS

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        main.cpp                             │
│  GLFW window + OpenGL context + main render loop            │
└────────┬────────────────────────────────────┬───────────────┘
         │                                    │
         ▼                                    ▼
┌─────────────────┐                 ┌──────────────────────┐
│  VideoInput     │                 │  StreamOutput        │
│  (FFmpeg decode)│                 │  (FFmpeg RTMP encode)│
│  local MP4/MKV  │                 │  → restream.io RTMP  │
└────────┬────────┘                 └──────────┬───────────┘
         │ decoded frames (YUV→RGB)            │ rendered frames
         ▼                                     │
┌─────────────────┐                            │
│  VideoTexture   │──────────────────────────▶ │
│  GL_TEXTURE_2D  │  upload to GPU each frame  │
└────────┬────────┘                            │
         │                                     │
         ▼                                     │
┌────────────────────────────────────────────┐ │
│              Renderer                      │ │
│  ┌─────────────────────────────────────┐   │ │
│  │          ShaderProgram              │   │ │
│  │  fractal.vert  +  fractal.frag      │   │ │
│  │                                     │   │ │
│  │  uniforms:                          │   │ │
│  │   u_blend_{mandelbrot,julia,        │   │ │
│  │            mandelbulb,euclidean}    │   │ │
│  │   u_julia_c, u_power, u_iterations  │   │ │
│  │   u_zoom, u_offset                  │   │ │
│  │   u_video_tex                       │   │ │
│  └─────────────────────────────────────┘   │ │
│                                            │ │
│  renders fullscreen quad → FBO             │◀┘
└────────────────────────────────────────────┘
         │ FBO pixels
         ▼
┌─────────────────┐    ┌──────────────────────┐
│  BlendController│    │  EquationEditor      │
│  (blend weights)│    │  (Dear ImGui panel)  │
│  FractalEngine  │    │  edit c, power,      │
│  (param store)  │    │  blend sliders,      │
└─────────────────┘    │  zoom/pan, RTMP URL  │
                       └──────────────────────┘
```

---

## Directory Structure

```
x/
├── CLAUDE.md
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── renderer/
│   │   ├── Renderer.h / Renderer.cpp
│   │   ├── ShaderProgram.h / ShaderProgram.cpp
│   │   └── VideoTexture.h / VideoTexture.cpp
│   ├── fractal/
│   │   ├── FractalEngine.h / FractalEngine.cpp
│   │   └── BlendController.h / BlendController.cpp
│   ├── stream/
│   │   ├── VideoInput.h / VideoInput.cpp      # FFmpeg AVFormatContext decode
│   │   └── StreamOutput.h / StreamOutput.cpp  # FFmpeg RTMP mux + encode
│   └── ui/
│       └── EquationEditor.h / EquationEditor.cpp  # Dear ImGui panels
├── shaders/
│   ├── fractal.vert          # passthrough quad vertex shader
│   ├── fractal.frag          # main blend shader (Mandelbrot + Julia + Euclidean)
│   └── mandelbulb.frag       # ray-marching 3D Mandelbulb
└── third_party/
    ├── imgui/                # Dear ImGui source (git submodule)
    └── glm/                  # GLM header-only math (git submodule)
```

---

## Dependencies

| Library | Purpose | Install |
|---------|---------|---------|
| GLFW 3 | Window + OpenGL context | `apt install libglfw3-dev` |
| GLEW | OpenGL extension loader | `apt install libglew-dev` |
| GLM | Vector/matrix math (header-only) | git submodule |
| FFmpeg (libav*) | Video decode + RTMP encode | `apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev` |
| Dear ImGui | Equation/blend editor UI | git submodule |
| OpenGL 4.3+ | GPU shader execution | driver |

---

## Build

```bash
# clone with submodules
git clone --recurse-submodules <repo>

# install system deps (Debian/Ubuntu)
sudo apt install cmake libglfw3-dev libglew-dev \
     libavcodec-dev libavformat-dev libavutil-dev libswscale-dev

# build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# run
./build/fractal_stream
```

---

## Signal / Data Flow Detail

### Video Input → GPU Texture
1. `VideoInput` opens any file FFmpeg can decode (MP4, MKV, MOV, etc.)
2. Each frame is decoded to `AVFrame` in `AV_PIX_FMT_RGB24` via `swscale`
3. `VideoTexture::upload(AVFrame*)` calls `glTexSubImage2D` every rendered frame
4. Shader samples `u_video_tex` using fractal-derived UV coordinates

### Fractal Blend in GLSL (`fractal.frag`)
```glsl
// Each mode returns a normalised [0,1] escape value
float m = mandelbrot(uv);
float j = julia(uv, u_julia_c);
float e = euclidean(uv);          // SDF of circles/polygons

// Blend weights are normalised so they sum to 1
float total = u_blend_m + u_blend_j + u_blend_mb + u_blend_e + 1e-6;
float escape = (u_blend_m*m + u_blend_j*j + u_blend_mb*mb + u_blend_e*e) / total;

// Map escape value to video UV
vec2 videoUV = vec2(escape, fract(escape * 7.3));
vec3 videoColor = texture(u_video_tex, videoUV).rgb;

fragColor = vec4(videoColor * colorize(escape), 1.0);
```

### Mandelbulb Ray-marching (`mandelbulb.frag`)
```glsl
// Distance Estimator — Inigo Quilez method
float DE_mandelbulb(vec3 pos) {
    vec3 z = pos;
    float dr = 1.0, r = 0.0;
    for (int i = 0; i < 64; i++) {
        r = length(z);
        if (r > 2.0) break;
        float theta = acos(z.z / r) * u_power;
        float phi   = atan(z.y, z.x) * u_power;
        float zr    = pow(r, u_power);
        dr = pow(r, u_power - 1.0) * u_power * dr + 1.0;
        z  = zr * vec3(sin(theta)*cos(phi), sin(theta)*sin(phi), cos(theta)) + pos;
    }
    return 0.5 * log(r) * r / dr;
}
```

### RTMP Output to Restream
1. `StreamOutput` opens an `AVFormatContext` with `rtmp://live.restream.io/live/<key>`
2. Uses `libx264` encoder (via `avcodec`) at configurable bitrate
3. Each rendered frame is read back from the FBO via `glReadPixels` → `AVFrame` → encoded → muxed
4. UI exposes: stream key field, bitrate slider, resolution selector

---

## Equation Editor (ImGui Panels)

### Panel: Blend
| Control | Type | Range | Effect |
|---------|------|-------|--------|
| Mandelbrot weight | slider | 0–1 | blend contribution |
| Julia weight | slider | 0–1 | blend contribution |
| Mandelbulb weight | slider | 0–1 | 3D ray-march blend |
| Euclidean weight | slider | 0–1 | SDF geometry blend |

### Panel: Iteration Formula
| Control | Type | Effect |
|---------|------|--------|
| Formula | combo (0–10) | selects iteration equation (see Formula Reference below) |
| Formula blend | slider 0–1 | 0 = pure z²+c, 1 = pure chosen formula, mix in between |

### Panel: Fractal Parameters
| Control | Type | Effect |
|---------|------|--------|
| Julia C (real, imag) | drag float2 | shift Julia set shape |
| Power (n) | drag float | Mandelbulb power / z^n exponent |
| Max iterations | int slider | detail level |
| Bailout radius | drag float | escape threshold |
| Zoom | scroll / drag | view scale |
| Pan offset | drag | view position |

### Panel: Euclidean Geometry
| Control | Type | Effect |
|---------|------|--------|
| Shape type | combo | Circle, Polygon, Star, Grid |
| N sides (polygon) | int | polygon vertex count |
| Radius | drag float | shape size |
| Rotation | drag float | shape angle |
| Repeat / tile | checkbox | tiled SDF |
| Orbit-trap warp | slider 0–1 | 0 = pure orbit-trap color, >0 = geometry bends the fractal orbit |

### Panel: 3-D Fractal
| Control | Type | Effect |
|---------|------|--------|
| 3-D type | combo | Mandelbulb / Mandelbox / Quaternion Julia |
| Mandelbox scale | drag float | IFS fold-and-scale factor (typ. 2.0) |
| Mandelbox fold | drag float | box fold limit |

### Panel: Stream Output
| Control | Type |
|---------|------|
| Destination list | add/remove rows | per-row RTMP URL + stream key (multi-stream) |
| Bitrate (kbps) | slider 1000–40000 | up to 40 Mbps for 4K |
| Output resolution | combo 720p/1080p/1440p/4K | |
| Start / Stop stream | button | |

---

## Shader Uniform Reference

```glsl
// ── Blend weights (normalised in-shader) ────────────────────────────────────
uniform float u_blend_mandelbrot;
uniform float u_blend_julia;
uniform float u_blend_mandelbulb;
uniform float u_blend_euclidean;

// ── Fractal iteration ────────────────────────────────────────────────────────
uniform vec2  u_julia_c;          // Julia c constant
uniform float u_power;            // Mandelbulb/z^n power
uniform int   u_max_iter;
uniform float u_bailout;
uniform float u_zoom;
uniform vec2  u_offset;
uniform float u_time;             // seconds since start (animation)

// ── Formula selector (fractal.frag) ─────────────────────────────────────────
uniform int   u_formula;          // 0–10, see Formula Reference
uniform float u_formula_blend;    // 0=pure z²+c  1=pure formula  (linear mix)

// ── Euclidean geometry / SDF ─────────────────────────────────────────────────
uniform int   u_geo_shape;        // 0=circle 1=polygon 2=star 3=grid
uniform int   u_geo_sides;
uniform float u_geo_radius;
uniform float u_geo_rotation;
uniform bool  u_geo_tile;
uniform float u_geo_warp;         // orbit-trap warp strength (0=coloring only)

// ── 3-D fractal (mandelbulb.frag) ────────────────────────────────────────────
uniform int   u_fractal_3d;       // 0=Mandelbulb 1=Mandelbox 2=Quaternion Julia
uniform float u_mb_scale;         // Mandelbox: IFS scale factor
uniform float u_mb_fold;          // Mandelbox: box fold limit

// ── Video ────────────────────────────────────────────────────────────────────
uniform sampler2D u_video_tex;
uniform vec2      u_video_size;

// ── Window ───────────────────────────────────────────────────────────────────
uniform vec2  u_resolution;
```

---

## Formula Reference (`u_formula`)

The iteration formula is selected with the `u_formula` uniform (integer 0–10).
`u_formula_blend` (0–1) linearly interpolates between the classic z²+c orbit and
the chosen formula, allowing smooth transitions in the UI.

| ID | Name | Recurrence | Notes |
|----|------|-----------|-------|
| 0 | **Mandelbrot / Julia** | zₙ₊₁ = zₙ² + c | Classic escape-time; Mandelbrot when z₀=0, Julia when z₀=pixel |
| 1 | **Sinus** | zₙ₊₁ = sin(zₙ) + c | Complex sine; produces flame-like symmetric filaments |
| 2 | **Exponential** | zₙ₊₁ = exp(zₙ) + c | Spiral arms; never fully bounded—bailout drives coloring |
| 3 | **Cosine** | zₙ₊₁ = cos(zₙ) + c | Twin of Sinus; different phase produces distinct textures |
| 4 | **Sinh** | zₙ₊₁ = sinh(zₙ) + c | Hyperbolic sine; elongated lobes along real axis |
| 5 | **Cosh** | zₙ₊₁ = cosh(zₙ) + c | Hyperbolic cosine; symmetric saddle shapes |
| 6 | **Burning Ship** | zₙ₊₁ = (|Re zₙ| + i|Im zₙ|)² + c | Absolute-value fold before squaring; ship silhouette at full zoom |
| 7 | **Tricorn / Mandelbar** | zₙ₊₁ = conj(zₙ)² + c | Conjugate before squaring; three-fold symmetry |
| 8 | **Newton z³−1** | zₙ₊₁ = zₙ − (zₙ³−1)/(3zₙ²) | Newton's method; convergence to three cube roots of unity; coloring by nearest root |
| 9 | **Phoenix** | zₙ₊₁ = zₙ² + Re(c) + Im(c)·zₙ₋₁ | Two-step memory recurrence; feather-like phoenix wings |
| 10 | **Power** | zₙ₊₁ = zₙⁿ + c | Arbitrary real power via polar form; uses `u_power` |

### Complex Number Library (GLSL)

The shader implements a full complex-math library (Fractal Explorer 2 / UltraFractal
function-set style) that all formulas build on:

```glsl
// Arithmetic
cmul(a,b)   csqr(z)   ccube(z)   cinv(z)   cdiv(a,b)   cconj(z)

// Exponential / power
cexp(z)     clog(z)
cpow_r(z,n)   // z^n via polar form  (real exponent)
cpow_c(z,w)   // z^w = exp(w·log z)  (complex exponent)

// Trigonometric (complex domain)
csin(z)   ccos(z)   ctan(z)

// Hyperbolic (complex domain)
csinh(z)   ccosh(z)   ctanh(z)

// Square root
csqrt(z)
```

### SDF Orbit-Trap Coupling

When `u_blend_euclidean > 0` the signed-distance field is evaluated **inside the
iteration loop** at each `zₙ`:

```
trap = min(trap, |SDF(zₙ)|)          // orbit-trap coloring
```

When `u_geo_warp > 0`, the SDF gradient additionally *moves* zₙ toward the shape
boundary on every step, algebraically coupling the Euclidean geometry into the
fractal orbit:

```
zₙ -= normalize(∇SDF(zₙ)) · sign(SDF(zₙ)) · warp · 0.035
```

A domain-warp pass also pre-distorts the complex plane *before* iteration begins
using the same gradient, doubling the visual coupling.

---

## Restream Setup

1. Login to [restream.io](https://restream.io) → **Add Channel**
2. Copy your **RTMP URL** and **Stream Key** from Dashboard → Stream Setup
3. Paste into the **Stream Output** ImGui panel
4. Default Restream ingest: `rtmp://live.restream.io/live/<your-key>`

---

## Performance Notes

- Mandelbulb ray-march is GPU-heavy; reduce `u_max_iter` (64 default) if frame rate drops
- FBO readback (`glReadPixels`) for RTMP encode stalls the GPU pipeline; use a PBO (Pixel Buffer Object) double-buffer for async readback at stream resolution
- Run video decode (`VideoInput`) on a dedicated thread; upload texture on the render thread via a mutex-protected ring buffer of 3 frames
- Target 60 fps render / 30 fps stream encode
