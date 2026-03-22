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

    // Animated color floor — gives the warp feedback loop something to work
    // with even on a cold start or a preset with no geometry of its own.
    // Three slow-rotating hue bands; amplitude kept low (≤0.18) so it never
    // overpowers preset colors once the loop has warmed up.
    vec2 uvC = uv - 0.5;
    float t  = u_time;
    float r  = length(uvC);
    float a  = atan(uvC.y, uvC.x);
    float floor_r = 0.18 + 0.17 * sin(t * 0.31 + a * 3.0 + r * 6.0);
    float floor_g = 0.14 + 0.16 * sin(t * 0.23 + a * 2.0 + r * 4.5 + 2.1);
    float floor_b = 0.22 + 0.18 * sin(t * 0.17 + a * 4.0 + r * 5.0 + 4.2);
    vec3  colorFloor = vec3(floor_r, floor_g, floor_b);

    // Composite: animated floor → warp feedback → waves → shapes.
    // The floor is always mixed in at a low level so the feedback loop
    // always has colorful content to distort; presets override via their
    // own brighter wave/shape layers.
    vec4 color = warp;
    float darkness = 1.0 - dot(color.rgb, vec3(0.333));
    color.rgb = mix(color.rgb, colorFloor, clamp(darkness * 0.55, 0.0, 0.5));
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
