#version 410 core
// MilkDrop composite pass — LINEAR output (no gamma).
// Gamma is applied separately in milkdrop_display.frag at blit-to-screen time.
// Keeping this linear prevents gamma from compounding each frame in the feedback loop.

in  vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_warp_tex;    // warped feedback (linear)
uniform sampler2D u_wave_tex;    // wave layer (RGBA, cleared each frame)
uniform sampler2D u_shape_tex;   // shape layer (RGBA, cleared each frame)
uniform sampler2D u_fractal_tex; // optional fractal overlay
uniform float u_fractal_blend;
uniform int   u_fractal_enabled; // 0 = off

void main() {
    vec2 uv = v_uv;

    vec4 warp   = texture(u_warp_tex,  uv);
    vec4 waves  = texture(u_wave_tex,  uv);
    vec4 shapes = texture(u_shape_tex, uv);

    // Composite: warp feedback → waves → shapes (alpha blended)
    vec4 color = warp;
    color.rgb = mix(color.rgb, waves.rgb,  waves.a);
    color.rgb = mix(color.rgb, shapes.rgb, shapes.a);

    // Optional fractal glow overlay (additive)
    if (u_fractal_enabled != 0) {
        vec4 fractal = texture(u_fractal_tex, uv);
        color.rgb += fractal.rgb * fractal.a * u_fractal_blend;
    }

    // Vignette — darkens edges slightly so they fade faster in the feedback loop,
    // concentrating activity toward the center (classic MilkDrop look).
    vec2 c = uv - 0.5;
    float vignette = 1.0 - dot(c, c) * 0.4;
    color.rgb *= vignette;

    // Output linear — NO gamma here. Gamma is in milkdrop_display.frag.
    fragColor = clamp(color, 0.0, 1.0);
}
