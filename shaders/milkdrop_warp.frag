#version 410 core
// MilkDrop warp pass — feedback decay + zoom/rotation/warp distortion
// Ported from MilkDrop.metal warp_fragment

in  vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_prev;   // previous composite frame (feedback)

// ── Warp uniforms ──────────────────────────────────────────────────────────
uniform float u_time;
uniform float u_zoom;
uniform float u_rot;
uniform float u_warp;
uniform float u_cx;
uniform float u_cy;
uniform float u_dx;
uniform float u_dy;
uniform float u_sx;
uniform float u_sy;
uniform float u_decay;
uniform float u_gamma;
uniform float u_aspect;    // width / height

void main() {
    vec2 uv = v_uv;

    // Map to centered space, apply aspect
    vec2 uvC = (uv - vec2(u_cx, u_cy)) * vec2(u_aspect, 1.0);

    // Zoom
    float zoom = max(u_zoom, 0.001);
    uvC /= zoom;

    // Rotation
    float c = cos(u_rot);
    float s = sin(u_rot);
    uvC = vec2(uvC.x * c - uvC.y * s,
               uvC.x * s + uvC.y * c);

    // Scale
    uvC *= vec2(u_sx, u_sy);

    // Translation
    uvC += vec2(u_dx, u_dy) * 2.0;

    // Warp (psychedelic swirl)
    float t = u_time * u_warp * 0.5;
    float warpX = sin(t * 1.11 + uvC.y * 3.0) * u_warp * 0.03;
    float warpY = cos(t * 0.93 + uvC.x * 2.5) * u_warp * 0.03;
    uvC += vec2(warpX, warpY);

    // Un-center
    vec2 sampleUV = uvC / vec2(u_aspect, 1.0) + vec2(u_cx, u_cy);

    // Sample with repeat wrap
    vec4 color = texture(u_prev, fract(sampleUV));

    // Gamma + decay
    color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(u_gamma));
    color.rgb *= u_decay;

    fragColor = color;
}
