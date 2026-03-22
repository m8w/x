#version 410 core
// MilkDrop warp pass — receives pre-computed positions + source UVs from CPU mesh
layout(location = 0) in vec2 a_pos;  // NDC position [-1,1]
layout(location = 1) in vec2 a_uv;   // pre-computed source UV (where to sample prev frame)

out vec2 v_uv;

void main() {
    gl_Position = vec4(a_pos, 0.0, 1.0);
    v_uv = a_uv;
}
