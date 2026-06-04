#version 410 core
// MilkDrop warp pass — sample previous frame at per-vertex UV, apply decay

in  vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_prev;
uniform float     u_decay;

void main() {
    fragColor = texture(u_prev, fract(v_uv)) * vec4(vec3(u_decay), 1.0);
}
