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
uniform vec4  u_ambient;         // preset per-frame r,g,b,a background color

void main() {
    vec2 uv = v_uv;

    vec4 warp   = texture(u_warp_tex,  uv);
    vec4 waves  = texture(u_wave_tex,  uv);
    vec4 shapes = texture(u_shape_tex, uv);

    // Preset ambient color — each preset's per_frame equations set r,g,b,a.
    // Use this as the color floor so presets produce their own color palette.
    // Fall back to a tiny neutral tint so the feedback loop never goes black.
    vec3 ambient = u_ambient.rgb * max(u_ambient.a, 0.1);
    ambient = max(ambient, vec3(0.04, 0.04, 0.06));  // minimum seed, neutral dark blue

    // Composite: preset ambient → warp feedback → waves → shapes.
    // Blend ambient into dark areas so the feedback loop has the preset's
    // own colors to work with. Bright warp content overrides naturally.
    vec4 color = warp;
    float darkness = 1.0 - dot(color.rgb, vec3(0.333));
    color.rgb = mix(color.rgb, ambient, clamp(darkness * 0.6, 0.0, 0.55));
    color.rgb = mix(color.rgb, waves.rgb,  waves.a);
    color.rgb = mix(color.rgb, shapes.rgb, shapes.a);

    // Fractal stream overlay (additive glow)
    if (u_fractal_enabled != 0) {
        vec4 fractal = texture(u_fractal_tex, uv);
        color.rgb += fractal.rgb * fractal.a * u_fractal_blend;
    }

    // MilkDrop gamma is a brightness multiplier.  Use Reinhard tonemapping so
    // high gamma values (e.g. fGammaAdj=1.7) brighten midtones without
    // blowing bright regions to solid white.
    float g = clamp(u_gamma * u_brightness, 0.1, 3.0);
    color.rgb = color.rgb * g / (1.0 + color.rgb * g);

    // Subtle vignette
    vec2 c = uv - 0.5;
    float vignette = 1.0 - dot(c, c) * 0.5;
    color.rgb *= vignette;

    fragColor = clamp(color, 0.0, 1.0);
}
