#version 410 core
// Fullscreen pass (paired with fractal.vert): copies the previous
// accumulation frame forward, scaled down by u_decay, before the new
// sample batch is additively blended on top. Keeps the image alive
// during a live stream instead of converging to a frozen long-exposure.
//
// Shared by both point-cloud accumulation pipelines: Buddhabrot's RGB32F
// density texture (alpha unused) and Flame's RGBA32F color+hitcount texture
// (where alpha is the hit count and must decay too) — decaying the full
// vec4 is correct for both.
in  vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_prev_tex;
uniform float     u_decay;

void main() {
    fragColor = texture(u_prev_tex, v_uv) * u_decay;
}
