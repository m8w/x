#version 410 core
// MilkDrop shape vertex shader
// Ported from MilkDrop.metal shape_vertex
// Triangle vertices are generated on the CPU and uploaded via VBO.

layout(location = 0) in vec2 a_pos;   // 0..1 screen UV space

out vec4 v_color;

uniform vec4  u_shape_color;    // inner color
uniform vec4  u_shape_color2;   // outer color
uniform vec2  u_shape_center;   // center in 0..1 space
uniform float u_shape_radius;

void main() {
    gl_Position = vec4(a_pos * 2.0 - 1.0, 0.0, 1.0);

    // Radial gradient: center → edge
    float dist = length(a_pos - u_shape_center);
    float t    = clamp(dist / max(u_shape_radius, 0.001), 0.0, 1.0);
    v_color    = mix(u_shape_color, u_shape_color2, t);
}
