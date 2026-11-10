#version 410 core
// ── Fractal Flame chaos-game sampler ──────────────────────────────────────────
//
// One invocation per launched walk (GL_POINTS in, GL_POINTS out). Each
// invocation runs an independent short chaos-game orbit, same parallel-GPU
// approach as buddhabrot_points.geom but a different iteration rule: instead
// of an escape-time orbit, every step picks one of up to 4 affine+variation
// "xforms" at random (weighted by u_xform_weight[]) and applies it — the
// classic fractal-flame algorithm (Scott Draves / flam3), curated down to 8
// cheap variations and 4 xform slots to stay real-time on a geometry shader.
//
// Unlike Buddhabrot there's no escape/non-escape split, so one pass suffices:
// a short discarded "burn-in" lets the orbit converge onto the attractor,
// then every subsequent step is plotted. Each plotted point also carries a
// blended color coordinate (the flam3 convention: color = mix(color,
// xformColor[picked], 0.5) every step) sampled through the shared IQ cosine
// palette.
//
// max_vertices = 128 for the same reason as buddhabrot_points.geom: GL only
// guarantees 1024 output components per invocation, and gl_Position (4) +
// g_color (3) = 7 components/vertex keeps 128 verts safely under that floor.
layout(points) in;
layout(points, max_vertices = 128) out;

flat in int v_id[];
out vec3 g_color;

uniform vec2  u_resolution;
uniform float u_zoom;
uniform vec2  u_offset;
uniform float u_seed;        // per-frame entropy value — new walks every frame

uniform int   u_steps;       // plotted iterations per walk (<=128)
uniform int   u_burn_in;     // discarded warm-up iterations before plotting

uniform int   u_xform_count;             // active xform slots (1-4)
uniform float u_xform_a[4], u_xform_b[4];
uniform float u_xform_c[4], u_xform_d[4];
uniform float u_xform_e[4], u_xform_f[4]; // affine: (a b e; c d f) applied to (x,y,1)
uniform int   u_xform_variation[4];       // 0=Linear 1=Sinusoidal 2=Spherical 3=Swirl
                                           // 4=Horseshoe 5=Polar 6=Handkerchief 7=Spiral
uniform float u_xform_weight[4];          // cumulative-normalised selection probabilities
uniform float u_xform_color[4];           // palette coordinate 0..1 per xform

const int CAP_HARD = 128;

// ── hash / pseudo-random (no GLSL builtin RNG — standard sin-hash) ────────────
float hash1(float n) { return fract(sin(n) * 43758.5453123); }
vec2  hash2(float n)  { return vec2(hash1(n), hash1(n + 17.231)); }

// IQ cosine palette — same one fractal.frag / buddhabrot_display.frag use.
vec3 palette(float t) {
    return vec3(0.5) + vec3(0.5) * cos(6.28318 * (vec3(1.0, 1.0, 0.5) * t + vec3(0.8, 0.9, 0.3)));
}

// Weighted pick among the active xform slots from a roll in [0,1).
// u_xform_weight[] is pre-normalised and cumulative on the CPU side, so this
// is a simple threshold walk; the last active slot catches any remainder.
int pickXform(float roll) {
    float acc = 0.0;
    for (int i = 0; i < u_xform_count; i++) {
        acc += u_xform_weight[i];
        if (roll < acc) return i;
    }
    return u_xform_count - 1;
}

vec2 applyAffine(int t, vec2 p) {
    return vec2(u_xform_a[t]*p.x + u_xform_b[t]*p.y + u_xform_e[t],
                u_xform_c[t]*p.x + u_xform_d[t]*p.y + u_xform_f[t]);
}

// Curated flam3 variation set — cheap, no loops, visually distinct.
vec2 applyVariation(int v, vec2 p) {
    if (v == 0) return p;   // Linear

    float r2    = dot(p, p);
    float r     = sqrt(max(r2, 1e-9));
    float theta = atan(p.y, p.x);

    if (v == 1) return sin(p);                                              // Sinusoidal
    if (v == 2) return p / max(r2, 1e-6);                                   // Spherical
    if (v == 3) { float s = sin(r2), c = cos(r2);                           // Swirl
                  return vec2(p.x*s - p.y*c, p.x*c + p.y*s); }
    if (v == 4) { float ri = 1.0 / max(r, 1e-6);                            // Horseshoe
                  return vec2((p.x - p.y) * (p.x + p.y) * ri, 2.0 * p.x * p.y * ri); }
    if (v == 5) return vec2(theta / 3.14159265, r - 1.0);                   // Polar
    if (v == 6) return r * vec2(sin(theta + r), cos(theta - r));            // Handkerchief
    float ri = 1.0 / max(r, 1e-6);                                          // Spiral
    return ri * vec2(cos(theta) + sin(r), sin(theta) - cos(r));
}

vec2 applyXform(int t, vec2 p) {
    return applyVariation(u_xform_variation[t], applyAffine(t, p));
}

// Same aspect/zoom/offset convention fractal.frag's main() uses.
vec2 worldToNDC(vec2 p) {
    vec2 aspect = vec2(u_resolution.x / u_resolution.y, 1.0);
    vec2 uv     = (p - u_offset) * u_zoom / (aspect * 2.0) + 0.5;
    return uv * 2.0 - 1.0;
}

void main() {
    float id = float(v_id[0]) * 7919.0 + u_seed * 104729.0;
    vec2  p  = (hash2(id) - 0.5) * 2.0;   // random start point in [-1,1]^2
    float colorCoord = hash1(id + 541.0);

    int steps  = min(u_steps, CAP_HARD);
    int burnIn = max(0, u_burn_in);

    // ── Burn-in: let the orbit converge onto the attractor, don't plot ──────
    for (int i = 0; i < burnIn; i++) {
        float roll = hash1(id + float(i) * 131.71 + 991.0);
        int   t    = pickXform(roll);
        p          = applyXform(t, p);
        colorCoord = mix(colorCoord, u_xform_color[t], 0.5);
    }

    // ── Plot pass: every remaining step is emitted ───────────────────────────
    for (int i = 0; i < steps; i++) {
        float roll = hash1(id + float(burnIn + i) * 131.71 + 991.0);
        int   t    = pickXform(roll);
        p          = applyXform(t, p);
        colorCoord = mix(colorCoord, u_xform_color[t], 0.5);

        gl_Position = vec4(worldToNDC(p), 0.0, 1.0);
        g_color     = palette(colorCoord);
        EmitVertex();
    }
    EndPrimitive();
}
