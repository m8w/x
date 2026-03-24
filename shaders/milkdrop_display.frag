#version 410 core
// MilkDrop display pass — applies gamma to the linear composite for screen output.
// This shader runs ONCE at display time and is NOT in the feedback loop.
// Keeping gamma out of the feedback loop prevents the compounding convergence to white.

in  vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_tex;
uniform float     u_gamma;   // from fGammaAdj (preset, default 2)

void main() {
    vec3 c = texture(u_tex, v_uv).rgb;
    // MilkDrop gamma: pow(x, 1/fGammaAdj). gamma=2 → sqrt, vivid colors.
    c = pow(max(c, vec3(0.0)), vec3(1.0 / max(u_gamma, 0.1)));
    fragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}
