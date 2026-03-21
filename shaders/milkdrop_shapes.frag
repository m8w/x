#version 410 core
// MilkDrop shape fragment shader
// Ported from MilkDrop.metal shape_fragment

in  vec4 v_color;
out vec4 fragColor;

void main() {
    fragColor = v_color;
}
