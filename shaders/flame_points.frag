#version 410 core
// Writes one accumulation sample: the palette color in RGB, 1.0 in alpha
// (hit count). Additively blended (glBlendFunc(GL_ONE, GL_ONE)) into the
// ping-pong RGBA32F accumulation texture by the caller, so RGB ends up
// holding the summed color and alpha the total hit count — exactly what
// flame_display.frag needs to average color and log-tone-map density.
in  vec3 g_color;
out vec4 fragColor;

void main() {
    fragColor = vec4(g_color, 1.0);
}
