#version 410 core
// Fullscreen pass (paired with fractal.vert): copies the previous
// accumulation frame forward, scaled down by u_decay, before the new
// sample batch is additively blended on top. Keeps the image alive
// during a live stream instead of converging to a frozen long-exposure.
in  vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_prev_tex;
uniform float     u_decay;

void main() {
    fragColor = vec4(texture(u_prev_tex, v_uv).rgb * u_decay, 1.0);
}
