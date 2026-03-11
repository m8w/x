#version 430 core

in  vec2 v_uv;
out vec4 fragColor;

// ── Uniforms ──────────────────────────────────────────────────────────────────
uniform vec2  u_resolution;
uniform float u_time;

// Blend weights (already normalised by CPU)
uniform float u_blend_mandelbrot;
uniform float u_blend_julia;
uniform float u_blend_mandelbulb;
uniform float u_blend_euclidean;

// Fractal params
uniform vec2  u_julia_c;
uniform float u_power;
uniform int   u_max_iter;
uniform float u_bailout;
uniform float u_zoom;
uniform vec2  u_offset;

// Euclidean geometry
uniform int   u_geo_shape;      // 0=circle 1=polygon 2=star 3=grid
uniform int   u_geo_sides;
uniform float u_geo_radius;
uniform float u_geo_rotation;
uniform bool  u_geo_tile;

// Video texture
uniform sampler2D u_video_tex;

// ── Complex arithmetic ────────────────────────────────────────────────────────
vec2 cmul(vec2 a, vec2 b) { return vec2(a.x*b.x - a.y*b.y, a.x*b.y + a.y*b.x); }
vec2 csqr(vec2 z)         { return vec2(z.x*z.x - z.y*z.y, 2.0*z.x*z.y); }

// ── Mandelbrot ────────────────────────────────────────────────────────────────
float mandelbrot(vec2 c) {
    vec2 z = vec2(0.0);
    for (int i = 0; i < u_max_iter; i++) {
        z = csqr(z) + c;
        if (dot(z, z) > u_bailout * u_bailout) {
            // Smooth colouring via log iteration count
            return float(i) / float(u_max_iter) - log2(log2(dot(z,z))) / float(u_max_iter);
        }
    }
    return 0.0;
}

// ── Julia ─────────────────────────────────────────────────────────────────────
float julia(vec2 z) {
    for (int i = 0; i < u_max_iter; i++) {
        z = csqr(z) + u_julia_c;
        if (dot(z, z) > u_bailout * u_bailout) {
            return float(i) / float(u_max_iter) - log2(log2(dot(z,z))) / float(u_max_iter);
        }
    }
    return 0.0;
}

// ── Mandelbulb (2-D slice via angle projection) ───────────────────────────────
// In the 2-D shader we compute a representative z=0 plane slice.
// The full 3-D ray-march version is in mandelbulb.frag.
float mandelbulb_slice(vec2 c) {
    float r  = length(c);
    float th = atan(c.y, c.x);
    vec2  z  = c;
    float dr = 1.0;
    for (int i = 0; i < u_max_iter; i++) {
        float zr  = pow(length(z), u_power);
        float ang = atan(z.y, z.x) * u_power;
        dr = pow(length(z), u_power - 1.0) * u_power * dr + 1.0;
        z  = zr * vec2(cos(ang), sin(ang)) + c;
        if (length(z) > u_bailout) {
            return float(i) / float(u_max_iter);
        }
    }
    return 0.0;
}

// ── Euclidean SDF geometry ────────────────────────────────────────────────────
float sdf_circle(vec2 p, float r) {
    return length(p) - r;
}

float sdf_polygon(vec2 p, int n, float r) {
    float a = atan(p.y, p.x) + u_geo_rotation;
    float s = 6.28318 / float(n);
    a = abs(mod(a, s) - s * 0.5);
    return length(p) * cos(a) - r;
}

float sdf_star(vec2 p, int n, float r) {
    float a  = atan(p.y, p.x) + u_geo_rotation;
    float s  = 6.28318 / float(n * 2);
    a = abs(mod(a, 2.0*s) - s);
    float r2 = r * 0.5;
    return length(p) * cos(a) - mix(r, r2, step(s * 0.5, a));
}

float sdf_grid(vec2 p, float cell) {
    vec2 q = mod(p, cell) - cell * 0.5;
    return min(abs(q.x), abs(q.y)) - cell * 0.05;
}

float euclidean_escape(vec2 p) {
    if (u_geo_tile) p = mod(p * 2.0, 2.0) - 1.0;
    float r = u_geo_radius;
    float d;
    if      (u_geo_shape == 0) d = sdf_circle (p, r);
    else if (u_geo_shape == 1) d = sdf_polygon(p, u_geo_sides, r);
    else if (u_geo_shape == 2) d = sdf_star   (p, u_geo_sides, r);
    else                       d = sdf_grid   (p, r);
    return clamp(1.0 - abs(d) * 8.0, 0.0, 1.0);
}

// ── Colour palette (IQ cosine palette) ───────────────────────────────────────
vec3 palette(float t) {
    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.263, 0.416, 0.557);
    return a + b * cos(6.28318 * (c * t + d));
}

// ── Main ──────────────────────────────────────────────────────────────────────
void main() {
    // Map pixel to complex plane
    vec2 aspect = vec2(u_resolution.x / u_resolution.y, 1.0);
    vec2 p = (v_uv - 0.5) * aspect * 2.0 / u_zoom + u_offset;

    // Per-mode escape values
    float em  = mandelbrot(p);
    float ej  = julia(p);
    float emb = mandelbulb_slice(p);
    float ee  = euclidean_escape(p);

    // Weighted blend
    float escape = u_blend_mandelbrot * em
                 + u_blend_julia      * ej
                 + u_blend_mandelbulb * emb
                 + u_blend_euclidean  * ee;
    escape = clamp(escape, 0.0, 1.0);

    // Map escape → video UV  (non-linear warp for visual interest)
    vec2 vidUV = vec2(
        fract(escape * 3.7 + u_time * 0.05),
        fract(escape * 5.3 + 0.5)
    );
    vec3 videoColor = texture(u_video_tex, vidUV).rgb;

    // Fractal colour tint blended with video
    vec3 fractalColor = palette(escape + u_time * 0.1);
    vec3 color = mix(fractalColor, videoColor, 0.65 + 0.35 * escape);

    // Darken interior (escape == 0)
    color *= step(0.001, escape) * 0.95 + 0.05;

    fragColor = vec4(color, 1.0);
}
