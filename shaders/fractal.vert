#version 430 core

// Fullscreen quad: no VBO needed, positions generated from gl_VertexID
out vec2 v_uv;   // [0,1] screen UV

void main() {
    vec2 pos = vec2((gl_VertexID & 1) * 2.0 - 1.0,
                    (gl_VertexID >> 1) * 2.0 - 1.0);
    v_uv = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
