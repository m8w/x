#version 410 core

// ── Spectral Post-Process ────────────────────────────────────────────────────
//
// Reads the fractal FBO texture and applies frequency-band-driven visual
// distortion.  Audio FFT band energy values drive the effect intensity:
//
//   Band 0 (bass):     chromatic aberration + large-scale color hue shift
//   Band 1 (low-mid):  spatial sine-wave displacement warp
//   Band 2 (high-mid): brightness / contrast pulse
//   Band 3 (high):     edge emboss / fine pixel shimmer
//
// Each band effect is scaled by the corresponding u_fft_vis_gain[] slot,
// allowing per-band visual intensity control independently of the audio.

in vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_fbo_tex;
uniform vec2      u_resolution;
uniform float     u_time;
uniform float     u_fft_band[4];      // live audio band energy 0..1
uniform float     u_fft_vis_gain[4];  // per-band visual gain multiplier

// ── Helpers ───────────────────────────────────────────────────────────────────

vec3 rgb2hsl(vec3 c) {
    float mx = max(c.r, max(c.g, c.b));
    float mn = min(c.r, min(c.g, c.b));
    float h  = 0.0, s = 0.0, l = (mx + mn) * 0.5;
    if (mx != mn) {
        float d = mx - mn;
        s = l > 0.5 ? d / (2.0 - mx - mn) : d / (mx + mn);
        if      (mx == c.r) h = (c.g - c.b) / d + (c.g < c.b ? 6.0 : 0.0);
        else if (mx == c.g) h = (c.b - c.r) / d + 2.0;
        else                h = (c.r - c.g) / d + 4.0;
        h /= 6.0;
    }
    return vec3(h, s, l);
}

float hue2rgb(float p, float q, float t) {
    if (t < 0.0) t += 1.0;
    if (t > 1.0) t -= 1.0;
    if (t < 1.0/6.0) return p + (q - p) * 6.0 * t;
    if (t < 1.0/2.0) return q;
    if (t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
    return p;
}

vec3 hsl2rgb(vec3 c) {
    if (c.y == 0.0) return vec3(c.z);
    float q = c.z < 0.5 ? c.z * (1.0 + c.y) : c.z + c.y - c.z * c.y;
    float p = 2.0 * c.z - q;
    return vec3(hue2rgb(p, q, c.x + 1.0/3.0),
                hue2rgb(p, q, c.x),
                hue2rgb(p, q, c.x - 1.0/3.0));
}

void main() {
    vec2 uv = v_uv;

    // ── Band 1 (low-mid): spatial warp ────────────────────────────────────────
    float low = u_fft_band[1] * u_fft_vis_gain[1];
    if (low > 0.001) {
        float warpX = sin(uv.y * 12.0 + u_time * 2.3) * low * 0.018;
        float warpY = cos(uv.x *  8.0 + u_time * 1.7) * low * 0.012;
        uv += vec2(warpX, warpY);
    }

    // ── Band 0 (bass): chromatic aberration ──────────────────────────────────
    float bass = u_fft_band[0] * u_fft_vis_gain[0];
    vec3 col;
    if (bass > 0.001) {
        vec2 off   = vec2(bass * 0.014, bass * 0.007);
        col.r = texture(u_fbo_tex, clamp(uv + off, 0.0, 1.0)).r;
        col.g = texture(u_fbo_tex, clamp(uv,       0.0, 1.0)).g;
        col.b = texture(u_fbo_tex, clamp(uv - off, 0.0, 1.0)).b;
    } else {
        col = texture(u_fbo_tex, clamp(uv, 0.0, 1.0)).rgb;
    }

    // ── Band 0 (bass): hue rotation ──────────────────────────────────────────
    if (bass > 0.001) {
        vec3 hsl = rgb2hsl(col);
        hsl.x    = fract(hsl.x + bass * 0.12);
        col      = hsl2rgb(hsl);
    }

    // ── Band 2 (high-mid): brightness pulse ───────────────────────────────────
    float mid = u_fft_band[2] * u_fft_vis_gain[2];
    if (mid > 0.001) {
        col *= 1.0 + mid * 0.5;
    }

    // ── Band 3 (high): edge emboss / shimmer ──────────────────────────────────
    float high = u_fft_band[3] * u_fft_vis_gain[3];
    if (high > 0.002) {
        vec2 px  = 1.0 / u_resolution;
        vec3 dx  = texture(u_fbo_tex, clamp(uv + vec2(px.x, 0.0), 0.0, 1.0)).rgb
                 - texture(u_fbo_tex, clamp(uv - vec2(px.x, 0.0), 0.0, 1.0)).rgb;
        vec3 dy  = texture(u_fbo_tex, clamp(uv + vec2(0.0, px.y), 0.0, 1.0)).rgb
                 - texture(u_fbo_tex, clamp(uv - vec2(0.0, px.y), 0.0, 1.0)).rgb;
        vec3 edge = sqrt(dx * dx + dy * dy);
        col += edge * high * 2.0;
    }

    fragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
