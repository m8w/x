#version 410 core
// MilkDrop composite pass — blends warp + waves + shapes + optional fractal overlay
// Ported from MilkDrop.metal composite_fragment

in  vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_warp_tex;    // warped feedback frame
uniform sampler2D u_wave_tex;    // wave layer (cleared each frame)
uniform sampler2D u_shape_tex;   // shape layer (cleared each frame)
uniform sampler2D u_fractal_tex; // fractal stream overlay (from existing Renderer FBO)

uniform float u_brightness;
uniform float u_gamma;
uniform float u_time;
uniform float u_fractal_blend;
uniform int   u_fractal_enabled; // 0 = off, 1 = on

void main() {
    vec2 uv = v_uv;

    vec4 warp   = texture(u_warp_tex,  uv);
    vec4 waves  = texture(u_wave_tex,  uv);
    vec4 shapes = texture(u_shape_tex, uv);

    // Composite: warp feedback → waves → shapes
    vec4 color = warp;
    color.rgb = mix(color.rgb, waves.rgb,  waves.a);
    color.rgb = mix(color.rgb, shapes.rgb, shapes.a);

    // Fractal stream overlay (additive glow)
    if (u_fractal_enabled != 0) {
        vec4 fractal = texture(u_fractal_tex, uv);
        color.rgb += fractal.rgb * fractal.a * u_fractal_blend;
    }

    // MilkDrop gamma: pow(x, 1/fGammaAdj).
    // With fGammaAdj=2 this is sqrt(x), which converts the feedback loop to
    // a bright steady state (sqrt(x)*decay = x  →  x≈decay²≈0.96).
    // This is what makes MilkDrop look vivid — Reinhard compresses toward grey.
    float gamma = max(u_gamma * u_brightness, 0.1);
    color.rgb   = pow(max(color.rgb, vec3(0.0)), vec3(1.0 / gamma));

    // Subtle vignette
    vec2 c = uv - 0.5;
    float vignette = 1.0 - dot(c, c) * 0.4;
    color.rgb *= vignette;

    fragColor = clamp(color, 0.0, 1.0);
}
