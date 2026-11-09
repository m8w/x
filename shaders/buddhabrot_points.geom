#version 410 core
// ── Buddhabrot / Nebulabrot orbit sampler ─────────────────────────────────────
//
// One invocation per launched sample (GL_POINTS in, GL_POINTS out). Each
// invocation:
//   1. Hashes its vertex ID + a per-frame entropy seed into a random c in
//      the current view window (same zoom/offset mapping fractal.frag uses).
//   2. Iterates the orbit z0=0, z_{n+1} = f(z_n) + c — optionally cycling
//      through a small set of "hybrid" formulas every N steps — to find
//      whether/when it escapes (pass 1, no emission).
//   3. If it escaped within [u_min_iter, cap], re-iterates from scratch,
//      this time emitting one GL_POINT per visited z (pass 2).
//
// Classic Buddhabrot rule: an orbit contributes ALL its visited points if
// (and only if) it eventually escapes — orbits that stay bound forever
// (the filled Mandelbrot set) contribute nothing. Running two passes per
// sample (instead of speculatively emitting during a single pass) is what
// makes that correct: you can't un-emit a GL_POINT once escape turns out
// to never happen.
//
// max_vertices is capped at 128: GL guarantees only 1024 output components
// per invocation (GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS minimum), and each
// emitted vertex here costs gl_Position (4) + g_weight (3) = 7 components,
// so 128 * 7 = 896 stays safely under that floor across drivers.
layout(points) in;
layout(points, max_vertices = 128) out;

flat in int v_id[];
out vec3 g_weight;

uniform vec2  u_resolution;
uniform float u_zoom;
uniform vec2  u_offset;
uniform float u_bailout;
uniform float u_seed;        // per-frame entropy value — new sample set every frame

uniform bool  u_nebulabrot;
uniform int   u_iter_r;      // primary cap (Buddhabrot) / longest threshold (Nebulabrot)
uniform int   u_iter_g;
uniform int   u_iter_b;
uniform int   u_min_iter;

uniform bool  u_hybrid;
uniform int   u_hybrid_list[4];
uniform int   u_hybrid_count;
uniform int   u_hybrid_cycle;

const int CAP_HARD = 128;

// ── hash / pseudo-random (no GLSL builtin RNG — standard sin-hash) ────────────
float hash1(float n) { return fract(sin(n) * 43758.5453123); }
vec2  hash2(float n)  { return vec2(hash1(n), hash1(n + 17.231)); }

// ── complex ops ────────────────────────────────────────────────────────────────
vec2 cmul(vec2 a, vec2 b) { return vec2(a.x*b.x - a.y*b.y, a.x*b.y + a.y*b.x); }
vec2 cconj(vec2 z)        { return vec2(z.x, -z.y); }

// ── Hybrid combination formula step ───────────────────────────────────────────
// 0=Mandelbrot z²+c  1=Burning Ship  2=Tricorn  3=Celtic
vec2 orbitStep(int formulaId, vec2 z, vec2 c) {
    if (formulaId == 1) { vec2 a  = vec2(abs(z.x), abs(z.y)); return cmul(a, a) + c; }
    if (formulaId == 2) { vec2 cz = cconj(z);                 return cmul(cz, cz) + c; }
    if (formulaId == 3) { vec2 z2 = cmul(z, z);                return vec2(abs(z2.x), z2.y) + c; }
    return cmul(z, z) + c;  // 0: Mandelbrot
}

int formulaAt(int iter) {
    if (!u_hybrid || u_hybrid_count <= 0) return 0;
    int idx = (iter / max(1, u_hybrid_cycle)) % u_hybrid_count;
    return u_hybrid_list[idx];
}

// Map a world-space complex point to clip-space NDC using the same
// aspect/zoom/offset convention as fractal.frag's main(): p = (uv-0.5)*aspect*2/zoom + offset.
vec2 worldToNDC(vec2 p) {
    vec2 aspect = vec2(u_resolution.x / u_resolution.y, 1.0);
    vec2 uv     = (p - u_offset) * u_zoom / (aspect * 2.0) + 0.5;
    return uv * 2.0 - 1.0;
}

void main() {
    float id  = float(v_id[0]) * 9781.0 + u_seed * 104729.0;
    vec2  aspect = vec2(u_resolution.x / u_resolution.y, 1.0);
    vec2  rnd    = hash2(id);
    vec2  c      = (rnd - 0.5) * aspect * 2.0 / u_zoom + u_offset;

    int capMax = min(u_iter_r, CAP_HARD);
    float bail2 = u_bailout * u_bailout;

    // ── Pass 1: find the escape step without emitting anything ──────────────
    vec2 z = vec2(0.0);
    int  escapeStep = -1;
    for (int i = 0; i < capMax; i++) {
        z = orbitStep(formulaAt(i), z, c);
        if (dot(z, z) > bail2) { escapeStep = i; break; }
    }

    // Never escaped (bound orbit — part of the filled set) or escaped too
    // fast (boring near-field point) — contributes nothing, standard rule.
    if (escapeStep < u_min_iter) { EndPrimitive(); return; }

    // ── Pass 2: re-walk the same deterministic orbit, emitting each point ───
    z = vec2(0.0);
    for (int i = 0; i < escapeStep; i++) {
        z = orbitStep(formulaAt(i), z, c);

        vec3 w = vec3(1.0);
        if (u_nebulabrot) {
            w = vec3(i < u_iter_r ? 1.0 : 0.0,
                      i < u_iter_g ? 1.0 : 0.0,
                      i < u_iter_b ? 1.0 : 0.0);
            if (w.r + w.g + w.b < 0.5) continue;   // below every threshold — skip
        }

        gl_Position = vec4(worldToNDC(z), 0.0, 1.0);
        g_weight    = w;
        EmitVertex();
    }
    EndPrimitive();
}
