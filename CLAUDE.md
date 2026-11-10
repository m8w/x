# CLAUDE.md — Fractal Video Stream Renderer

## Project Overview

Real-time fractal renderer that:
- Decodes **local video files** as live input texture
- Blends **Mandelbrot, Julia, Mandelbulb, Mandelbox, Quaternion Julia, 3D Complex (Peng), Kleinian Limit Set** with **Euclidean geometry** via a live equation editor
- Supports **11 iteration formulas** (classic z²+c, Burning Ship, Newton, Phoenix, trig/hyperbolic, arbitrary power) inspired by the Fractal Explorer / UltraFractal function set
- Couples SDF geometry into the fractal orbit via **orbit-trap coloring** and **domain warp**
- Maps the video stream onto the fractal surface using escape-time UV mapping
- Renders **Buddhabrot / Nebulabrot** as a GPU point-cloud orbit-accumulation mode, with optional **hybrid combination iteration** (cycling formulas mid-orbit)
- Renders **Flame / IFS chaos-game** fractals as a second GPU point-cloud mode — up to 4 affine+variation "xforms," weighted random walk, flam3-style log-density color averaging
- Layers an optional **Quaternion Feedback** post-process trail — a non-commutative 3-D rotation (two independent rates) drives a UV-warped self-sampling feedback loop, stacking on top of any other mode
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

### Buddhabrot / Nebulabrot Pipeline (`buddhabrot_*.vert/.geom/.frag`)

Structurally different from every other fractal mode: instead of one escape
value per screen pixel, it samples many random `c` values per frame, iterates
each orbit, and plots **every point the orbit visits** (not just the final
escape value) into a persistent accumulation texture. Density there — not a
per-pixel formula — is what gets displayed. macOS caps OpenGL at 4.1 core, so
there's no compute-shader stage available; the whole thing runs as three
fullscreen/point passes through `Renderer::renderBuddhabrot()`:

1. **Decay pass** (`buddhabrot_decay.frag`) — multiplies the previous
   accumulation frame by `buddhaDecay` so the image fades instead of
   freezing into a static long-exposure photo (skipped in favor of a hard
   `glClear` when `buddhaResetRequested` is set, e.g. after a zoom/iteration
   change).
2. **Points pass** (`buddhabrot_points.vert/.geom/.frag`) — one GPU thread
   per sample. A geometry shader is the only way to emit a *variable* number
   of points per invocation without a compute stage, so each invocation:
   - hashes its `gl_VertexID` + a per-frame entropy seed into a random `c`
     in the current zoom/offset view window,
   - **pass 1** iterates the orbit (optionally cycling through up to 4
     "hybrid" formulas — Mandelbrot/Burning Ship/Tricorn/Celtic — every N
     steps) purely to find whether/when it escapes, emitting nothing,
   - **pass 2**, only if it escaped, re-walks the same deterministic orbit
     from scratch and calls `EmitVertex()` for every visited point.
   Two passes per sample (not one) is what makes the classic Buddhabrot rule
   correct — an orbit that never escapes must contribute *nothing*, and you
   can't un-emit a point once pass 1 tells you escape never happens.
   Points are additively blended (`GL_ONE, GL_ONE`) into an `RGB32F` texture.
   `max_vertices = 128` in the geometry shader is a hard GPU limit: GL only
   guarantees 1024 output components per invocation, and each emitted point
   costs ~7 (`gl_Position` + the RGB channel-mask), so iteration caps are
   clamped to 128 everywhere (`buddhaIterR/G/B`).
3. **Display pass** (`buddhabrot_display.frag`) — tone-maps the raw hit-count
   texture (`pow(density * brightness, gamma)`, gamma ~0.45) and writes it
   into the normal `m_fbo`/`m_fboTex`, so Buddhabrot output flows through the
   existing spectral post-process and stream/record readback unchanged.

**Nebulabrot** reuses this exact pipeline with `nebulabrot=true`: the R/G/B
channels are independent iteration-cap thresholds applied to the *same*
sample set in one pass (no 3x cost) — R the most permissive (rare, deep
filigree), B the tightest (common, broad structure) — composited straight
into the RGB accumulation texture instead of being recolored through the
palette.

Perf/quality is dominated by `buddhaSamples` (samples launched per frame);
drop that first if frame rate suffers. The sampling box always matches the
current `zoom`/`offset`, same convention `fractal.frag`'s `main()` uses, so
"zoom into the fractal" and "zoom into the Buddhabrot" behave the same way —
and any such change auto-resets the accumulation buffer, since the sampling
domain just moved.

### Kleinian Limit Set (`mandelbulb.frag`, `u_fractal_3d == 4`)

Same box-fold-and-scale IFS loop as `DE_mandelbox` above — reuses its
`u_mb_scale`/`u_mb_fold` uniforms — with one swap: Mandelbox's two-tier
fixed-radius ball fold (`if (r2<0.25) ... else if (r2<1.0) ...`) is replaced
by a single *continuous* sphere inversion, `z *= max(minR2/dot(z,z), 1.0)`.
That one change is the well-documented "Grandma's formula" construction from
fractal-art circles — a sphere inversion is a generator of a Möbius
transformation group, so iterating it is iterating a Kleinian group, and the
result is the lace-like circle-packing limit set instead of Mandelbox's boxy
structures. `u_kleinian_min_r2` (the inversion radius²) is the only
Kleinian-specific parameter.

### Flame / IFS Chaos-Game Pipeline (`flame_*.vert/.geom/.frag`)

A second point-cloud accumulation mode, reusing Buddhabrot's architecture
(ping-pong accumulation texture, decay pass, geometry-shader points pass,
tone-mapped display pass into `m_fbo`) but with the classic fractal-flame
(Scott Draves / flam3) iteration rule instead of an escape-time orbit:

- **No escape/non-escape split, so one pass per sample suffices** (unlike
  Buddhabrot's two-pass trick): `flame_points.geom` runs a short discarded
  "burn-in" so the walk converges onto the attractor, then plots every
  subsequent step.
- At each step, one of up to 4 affine+variation **"xforms"** is picked at
  random, weighted by `flameWeight[]` (normalised and uploaded as cumulative
  buckets so the shader's pick is a cheap threshold walk). A curated set of
  8 cheap, visually distinct variations is implemented (Linear, Sinusoidal,
  Spherical, Swirl, Horseshoe, Polar, Handkerchief, Spiral) — not flam3's
  full ~100, to stay real-time on a geometry shader.
- A running color coordinate blends toward each picked xform's
  `flameColor[]` every step (`mix(color, xformColor, 0.5)`, the standard
  flam3 convention), sampled through the shared IQ cosine palette and
  emitted per point as `g_color`.
- The accumulation texture is **RGBA32F**, not Buddhabrot's plain RGB32F:
  RGB sums the emitted color, alpha sums the hit count. `buddhabrot_decay.frag`
  was generalised to decay the full `vec4` (harmless for Buddhabrot, whose
  texture has no alpha channel) specifically so both pipelines could share
  one decay shader.
- `flame_display.frag` divides RGB by alpha to get the *average* color per
  pixel, then drives brightness from `log(1 + hitcount * brightness)` rather
  than a straight power curve — that log-density tone map is what gives
  flame fractals their glowing-highlight-with-visible-shadow-detail look.

The "Randomize xforms" UI button samples new affine coefficients from
`globalEntropy()` (the dual hardware/quantum RNG) within a range chosen to
keep the IFS contractive, since chaos-game convergence requires it — an
expansive xform set just renders as an empty/dark frame, not a crash.

### Quaternion Feedback (`quat_feedback.frag`)

Unlike every mode above, this is a **post-process pass, not an exclusive
mode** — `Renderer::render()` chains it right after the spectral pass, on
top of whatever `outputTex` already holds (2-D blend, raymarch, Buddhabrot,
Flame, even Distortion), so it composes with everything instead of
replacing it.

It originated from a user-supplied framework describing "thought streams"
as quaternions (`w, i, j, k`) navigating a "True Time" complex plane where
`Cause + Effect = 1`. That framing is evocative, not physics the code can
assert as fact, so here's the honest translation: the one piece of genuinely
well-defined math in it — a quaternion as an axis-angle 3-D rotation,
applied via `q·v·q⁻¹` — is real and implementable, and that's exactly what
got built. "Real time" and "imaginary time" survive only as the *names* of
two independent rotation rates (`quatRealRate`, `quatImagRate`); there's no
literal second time axis.

Mechanically: each frame, the previous feedback frame (`m_quatTex[readIdx]`,
a ping-pong pair parallel to Buddhabrot/Flame's) is re-sampled at a UV
offset equal to a rotated probe vector's XY. Two quaternion rotations —
same `quatAxis` family, cyclically permuted so they share no single axis,
driven at the two independent rates — are composed with `qMul` before being
applied. Because quaternion multiplication doesn't commute, that composed
rotation never settles into a flat, repeating 2-D spin; the resulting
feedback trail braids and crosses itself instead. The sampled trail is
decayed (`quatFeedbackDecay`) and blended on top of (never replacing) the
fresh render, then written to the write half of the ping-pong pair.

This is a single stateless fragment pass — no geometry shader, no
two-step decay-then-accumulate like Buddhabrot/Flame need — so it's cheap
enough to leave running continuously. `fboPixels()` picks its output for
stream/record encoding whenever it's active, same precedence the spectral
pass already had (`m_quatActive` is captured once per `render()` call since
`fboPixels()` has no direct access to `FractalEngine`).

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
| 3-D type | combo | Mandelbulb / Mandelbox / Quaternion Julia / 3D Complex (Peng) / Kleinian Limit Set |
| Mandelbox scale | drag float | IFS fold-and-scale factor (typ. 2.0) — shared with Kleinian |
| Mandelbox fold | drag float | box fold limit — shared with Kleinian (box half-size) |
| Kleinian min R² | drag float | sphere-inversion radius² threshold — the one Kleinian-specific parameter |

### Panel: Buddhabrot / Nebulabrot
| Control | Type | Effect |
|---------|------|--------|
| Enable Buddhabrot Mode | checkbox | exclusive mode switch — bypasses the 2-D blend / 3-D raymarch / distortion / flame paths entirely |
| Nebulabrot (RGB) | checkbox | off = single-channel density through the shared palette; on = R/G/B use independent iteration-cap thresholds |
| Samples / frame | slider 1000–60000 | random orbits launched per frame — primary perf/quality dial |
| Iter R / G / B | slider 1–128 | iteration caps, hard-limited by the GPU geometry-shader `max_vertices` budget |
| Min iter | slider 0–20 | skip orbits escaping before this step (cuts boring near-field points) |
| Decay | slider 0.90–1.0 | per-frame buffer fade; 1.0 = classic long-exposure, lower keeps it alive on a live stream |
| Brightness / Gamma | sliders | tone-map gain / curve for the sparse, high-dynamic-range density field |
| Reset accumulation | button | force-clears the buffer (zoom/iteration/hybrid changes already auto-reset it) |
| Cycle formulas | checkbox | hybrid combination iteration — switches the orbit formula every N steps |
| Switch every / Formula count / per-slot formula | slider / slider / combo | hybrid cycle configuration (Mandelbrot / Burning Ship / Tricorn / Celtic) |

### Panel: Flame / IFS Chaos Game
| Control | Type | Effect |
|---------|------|--------|
| Enable Flame Mode | checkbox | exclusive mode switch — bypasses the 2-D blend / 3-D raymarch / distortion / Buddhabrot paths entirely |
| Walks / frame | slider 1000–60000 | independent chaos-game walks launched per frame — primary perf/quality dial |
| Steps / walk | slider 4–128 | plotted iterations per walk, same GPU geometry-shader budget as Buddhabrot |
| Burn-in | slider 0–50 | discarded warm-up iterations so each walk starts on the attractor |
| Decay / Brightness / Gamma | sliders | buffer fade + log-density tone-map shaping (see architecture notes above) |
| Reset accumulation | button | force-clears the buffer |
| Randomize xforms | button | entropy-seeded random affine+variation set, kept contractive so the chaos game converges |
| Active xforms | slider 1–4 | how many of the 4 xform slots participate |
| a/b, c/d, e/f (translate) | drag float2 ×3 per xform | the xform's affine transform (2×2 linear + translate) |
| Variation | combo per xform | Linear / Sinusoidal / Spherical / Swirl / Horseshoe / Polar / Handkerchief / Spiral |
| Weight | slider per xform | selection probability at each chaos-game step (auto-normalised) |
| Color | slider per xform | palette coordinate the running walk color blends toward when picked |

### Panel: Quaternion Feedback
| Control | Type | Effect |
|---------|------|--------|
| Enable Quaternion Feedback | checkbox | post-process only — not exclusive, layers on top of any other mode |
| Amount | slider 0–1 | 0 = fractal only, 1 = feedback trail dominates (adds on top, never blanks) |
| Decay | slider 0.80–0.999 | per-frame trail fade; lower = shorter trails |
| Warp | slider 0–0.1 | UV-offset magnitude from the rotation |
| Real rate / Imaginary rate | sliders ±3 | two rotation rates composed non-commutatively (see architecture notes) |
| i / j / k axis | drag float3 | quaternion rotation axis weights, normalised in-shader |

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
uniform int   u_fractal_3d;       // 0=Mandelbulb 1=Mandelbox 2=Quaternion Julia 3=3D Complex (Peng) 4=Kleinian
uniform float u_mb_scale;         // Mandelbox / Kleinian: IFS scale factor
uniform float u_mb_fold;          // Mandelbox / Kleinian: box fold limit / half-size
uniform float u_kleinian_min_r2;  // Kleinian: sphere-inversion radius^2 threshold

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
