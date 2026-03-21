#version 410 core

// ── Distortion 2a — Iridescent Metaballs ─────────────────────────────────────
//
// Renders N animated metaballs that merge via smooth-minimum (Quilez smin).
// Coloring uses a per-blob iridescent palette driven by surface normals.
//
// Uniforms:
//   u_resolution    window size in pixels
//   u_time          seconds since start
//   u_dist_speed    animation rate multiplier   (0.1–3)
//   u_dist_blobs    number of metaballs         (3–10)
//   u_dist_glow     outer aura intensity        (0–2)
//   u_dist_irid     rainbow cycle frequency     (0.5–4)
//   u_dist_outline  edge ring brightness        (0–1)

uniform vec2  u_resolution;
uniform float u_time;
uniform float u_dist_speed;
uniform int   u_dist_blobs;
uniform float u_dist_glow;
uniform float u_dist_irid;
uniform float u_dist_outline;

out vec4 fragColor;

// ── Helpers ───────────────────────────────────────────────────────────────────

// Smooth minimum — merges two SDF values with a smooth blend region of radius k
float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5*(b - a)/k, 0.0, 1.0);
    return mix(b, a, h) - k*h*(1.0 - h);
}

// Iridescent RGB from a 0-1 phase `t` and a per-layer phase shift.
// Three sine waves offset by 120° (2π/3) span the full rainbow.
vec3 iridColor(float t, float layer) {
    float base = t * 6.2832 * u_dist_irid + layer * 1.3;
    return vec3(
        0.5 + 0.5*sin(base + 0.0),
        0.5 + 0.5*sin(base + 2.094),
        0.5 + 0.5*sin(base + 4.189)
    );
}

// Deterministic 2-component hash for deriving per-blob parameters
vec2 hash2(float n) {
    return fract(sin(vec2(n, n + 17.31)) * vec2(43758.54, 23421.63));
}

// Blob center + radius at time t for blob index i
void blobParams(int i, float t,
                out vec2 center, out float radius) {
    float fi   = float(i);
    vec2  h    = hash2(fi);
    float phase  = h.x * 6.2832;
    float speed1 = 0.31 + h.y  * 0.4;
    float speed2 = 0.19 + h.x  * 0.35;
    float rx     = 0.28 + h.y  * 0.22;
    float ry     = 0.20 + h.x  * 0.18;

    // Lissajous-like orbit + slow drift
    center = vec2(rx * sin(t*speed1 + phase),
                  ry * cos(t*speed2 + phase*1.3));
    center += 0.08 * vec2(sin(t*0.13 + fi), cos(t*0.11 + fi*2.0));

    // Pulsing radius
    radius = 0.14 + 0.07 * sin(t*0.7 + phase);
}

// ── SDF field evaluation (factored out to avoid loop duplication) ─────────────
//
// Original HTML shader duplicated the blob loop 3 times: once for the main
// field, and twice more (inline) to estimate the surface normal via forward
// differences. extracting evalField() means the gradient costs 2 extra function
// calls instead of 2 full copy-pasted loop bodies, eliminating that redundancy.

float evalField(vec2 p, float t) {
    float field = 1e6;
    int N = u_dist_blobs;
    for (int i = 0; i < 10; i++) {
        if (i >= N) break;
        vec2 center; float radius;
        blobParams(i, t, center, radius);
        field = smin(field, length(p - center) - radius, 0.12);
    }
    return field;
}

// ── Main ──────────────────────────────────────────────────────────────────────

void main() {
    // Normalised screen coordinates: (0,0) at centre, aspect-corrected
    vec2 uv = (gl_FragCoord.xy - 0.5*u_resolution) / min(u_resolution.x, u_resolution.y);
    float t  = u_time * u_dist_speed;

    // ── Primary loop: blob field + weighted iridescent colour ─────────────────
    float field       = 1e6;
    float totalWeight = 0.0;
    vec3  blobColor   = vec3(0.0);
    int   N           = u_dist_blobs;

    for (int i = 0; i < 10; i++) {
        if (i >= N) break;
        float fi = float(i);
        vec2  center; float radius;
        blobParams(i, t, center, radius);

        float dist = length(uv - center) - radius;
        field = smin(field, dist, 0.12);

        // Inverse-square weighting: closer blobs dominate the colour mix
        float w      = 1.0 / (dist*dist + 0.001);
        float colorT = sin(t*0.4 + fi*1.618)*0.5 + 0.5;
        blobColor   += iridColor(colorT, fi*0.8) * w;
        totalWeight += w;
    }
    blobColor /= max(totalWeight, 0.001);

    // ── Surface normal via forward finite differences ─────────────────────────
    // Two evalField calls (vs two full inline loop copies in the original).
    const float EPS = 0.003;
    float dx = evalField(uv + vec2(EPS, 0.0), t) - field;
    float dy = evalField(uv + vec2(0.0, EPS), t) - field;
    vec2  normal      = normalize(vec2(dx, dy) + 1e-5);
    float normalAngle = atan(normal.y, normal.x) / 6.2832 + 0.5;

    // ── Background iridescent gradient ────────────────────────────────────────
    float bgAngle = atan(uv.y, uv.x) / 6.2832 + 0.5;
    float bgT     = length(uv)*0.5 + t*0.08;
    vec3  bgColor = iridColor(bgAngle + bgT*0.15, 3.0) * 0.35;
    bgColor = mix(bgColor, vec3(0.05, 0.02, 0.08), 0.5);

    // ── Interior shading ──────────────────────────────────────────────────────
    float inside = smoothstep( 0.01, -0.04, field);
    float depth  = 1.0 - clamp(field / -0.15, 0.0, 1.0);
    float rim    = pow(1.0 - depth, 2.5);

    vec3 surfaceColor = blobColor;
    surfaceColor  = mix(surfaceColor, iridColor(normalAngle + t*0.06, 1.0), 0.55);
    surfaceColor *= 0.6 + 0.4*depth;                                          // depth shading
    surfaceColor += iridColor(normalAngle*2.0 + t*0.04, 2.5) * rim * 0.6;   // rim light

    // ── Chromatic outline rings ────────────────────────────────────────────────
    float edgeDist     = abs(field);
    float outlineW     = 0.025;
    float edgeMask     = smoothstep(outlineW, 0.0, edgeDist);
    float outlineAngle = normalAngle + t*0.1;
    vec3  outlineColor = iridColor(outlineAngle, 0.5) * 1.8;
    // Second narrower ring just inside the main edge
    float ring2 = smoothstep(outlineW*2.5, outlineW*1.2, edgeDist)
                * smoothstep(0.0, outlineW*0.8, edgeDist - 0.002);
    outlineColor += iridColor(outlineAngle + 0.33, 1.5) * ring2 * 1.2;

    // ── Glow aura ─────────────────────────────────────────────────────────────
    float aura     = exp(-max(field, 0.0) * 12.0) * u_dist_glow;
    vec3  glowColor = iridColor(normalAngle + t*0.05, 0.0) * aura;

    // ── Composite ─────────────────────────────────────────────────────────────
    vec3 color = bgColor;
    color  = mix(color, surfaceColor, inside);
    color += outlineColor * edgeMask * u_dist_outline * (0.3 + 0.7*rim);
    color += glowColor * 0.5;

    // Vignette
    color *= 1.0 - 0.4*smoothstep(0.5, 1.0, length(uv));

    // Gamma curve (matches original HTML canvas output)
    color = pow(clamp(color, 0.0, 1.0), vec3(0.88));

    fragColor = vec4(color, 1.0);
}
