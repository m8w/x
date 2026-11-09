#version 410 core
// Fullscreen pass (paired with fractal.vert): tone-maps the raw hit-count
// accumulation texture into a displayable image and writes it into the
// main fractal FBO, so Buddhabrot/Nebulabrot output flows through the
// existing spectral/stream/record pipeline exactly like any other mode.
in  vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_accum_tex;
uniform bool      u_nebulabrot;
uniform float     u_brightness;
uniform float     u_gamma;         // < 1 compresses the sparse, high-dynamic-range density field

// IQ cosine palette — same one fractal.frag uses, duplicated here since
// GLSL has no #include and each shader file in this project is self-contained.
vec3 palette(float t) {
    return vec3(0.5) + vec3(0.5) * cos(6.28318 * (vec3(1.0, 1.0, 0.5) * t + vec3(0.8, 0.9, 0.3)));
}

void main() {
    vec3 density = texture(u_accum_tex, v_uv).rgb * u_brightness;
    vec3 toned   = pow(clamp(density, 0.0, 1.0e6), vec3(u_gamma));

    vec3 color;
    if (u_nebulabrot) {
        color = clamp(toned, 0.0, 1.0);
    } else {
        float d = clamp(toned.r, 0.0, 1.0);
        color   = palette(d) * d;
    }

    fragColor = vec4(color, 1.0);
}
