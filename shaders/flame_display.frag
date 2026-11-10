#version 410 core
// Fullscreen pass (paired with fractal.vert): turns the raw RGBA32F
// accumulation texture (RGB = summed color, A = hit count) into a
// displayable image using the classic fractal-flame "log-density" tone map —
// average color per pixel (RGB / A), brightness driven by log(1 + A), not a
// straight linear or gamma count. That log curve is what gives flame
// fractals their glowing-highlight-with-visible-detail-in-the-dark-areas
// look instead of either crushed blacks or a blown-out core.
in  vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_accum_tex;
uniform float     u_brightness;
uniform float     u_gamma;   // extra post-curve shaping on top of the log tone map

void main() {
    vec4  accum = texture(u_accum_tex, v_uv);
    float count = accum.a;

    if (count < 1e-6) { fragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }

    vec3  avgColor = accum.rgb / count;                 // flam3 color averaging
    float density  = log(1.0 + count * u_brightness);    // log-density tone map
    density        = pow(clamp(density, 0.0, 1.0e6), u_gamma);

    fragColor = vec4(clamp(avgColor * density, 0.0, 1.0), 1.0);
}
