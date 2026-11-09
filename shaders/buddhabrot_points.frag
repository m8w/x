#version 410 core
// Writes one accumulation sample. Additively blended (glBlendFunc(GL_ONE,
// GL_ONE)) into the ping-pong RGB32F accumulation texture by the caller.
in  vec3 g_weight;
out vec4 fragColor;

void main() {
    fragColor = vec4(g_weight, 1.0);
}
