#version 410 core
// MilkDrop wave vertex shader
// Ported from MilkDrop.metal wave_vertex
// Positions come from a VBO (layout location 0); color from a uniform.

layout(location = 0) in vec2 a_pos;   // 0..1 screen UV space

out vec4 v_color;

uniform vec4 u_wave_color;

void main() {
    // Map [0,1] → [-1,1] clip space
    gl_Position = vec4(a_pos * 2.0 - 1.0, 0.0, 1.0);
    v_color = u_wave_color;
}
