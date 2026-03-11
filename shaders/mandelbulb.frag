#version 430 core
// Full 3-D Mandelbulb via sphere ray-marching.
// Used when u_blend_mandelbulb > 0.5 (switched in by Renderer).

in  vec2 v_uv;
out vec4 fragColor;

uniform vec2  u_resolution;
uniform float u_time;
uniform float u_power;
uniform int   u_max_iter;
uniform float u_zoom;
uniform sampler2D u_video_tex;

// ── Distance Estimator (Inigo Quilez method) ──────────────────────────────────
float DE(vec3 pos) {
    vec3  z  = pos;
    float dr = 1.0;
    float r  = 0.0;
    for (int i = 0; i < 64; i++) {
        r = length(z);
        if (r > 2.0) break;
        float theta = acos(z.z / r) * u_power;
        float phi   = atan(z.y, z.x) * u_power;
        float zr    = pow(r, u_power);
        dr = pow(r, u_power - 1.0) * u_power * dr + 1.0;
        z  = zr * vec3(sin(theta)*cos(phi),
                       sin(theta)*sin(phi),
                       cos(theta)) + pos;
    }
    return 0.5 * log(r) * r / dr;
}

// ── Ray-march ─────────────────────────────────────────────────────────────────
float march(vec3 ro, vec3 rd, out int steps) {
    float t = 0.0;
    steps = 0;
    for (int i = 0; i < 256; i++) {
        steps = i;
        float d = DE(ro + rd * t);
        if (d < 0.0005) return t;
        t += d;
        if (t > 4.0) break;
    }
    return -1.0;
}

// ── Normal via central differences ───────────────────────────────────────────
vec3 normal(vec3 p) {
    vec2 e = vec2(0.001, 0.0);
    return normalize(vec3(
        DE(p + e.xyy) - DE(p - e.xyy),
        DE(p + e.yxy) - DE(p - e.yxy),
        DE(p + e.yyx) - DE(p - e.yyx)
    ));
}

// ── Main ──────────────────────────────────────────────────────────────────────
void main() {
    vec2 uv = (v_uv - 0.5) * vec2(u_resolution.x / u_resolution.y, 1.0);

    // Orbit camera
    float ang = u_time * 0.2;
    vec3 ro = vec3(sin(ang) * 2.4, 0.5, cos(ang) * 2.4) / u_zoom;
    vec3 target = vec3(0.0);
    vec3 fw = normalize(target - ro);
    vec3 ri = normalize(cross(fw, vec3(0.0, 1.0, 0.0)));
    vec3 up = cross(ri, fw);
    vec3 rd = normalize(uv.x * ri + uv.y * up + 1.8 * fw);

    int steps;
    float t = march(ro, rd, steps);

    vec3 color;
    if (t > 0.0) {
        vec3 p = ro + rd * t;
        vec3 n = normal(p);
        vec3 light = normalize(vec3(1.0, 2.0, 1.5));
        float diff = max(dot(n, light), 0.0);
        float ao   = 1.0 - float(steps) / 256.0;

        // Map position to video UV
        vec2 vidUV = fract(p.xy * 0.5 + 0.5);
        vec3 videoColor = texture(u_video_tex, vidUV).rgb;

        color = videoColor * (diff * ao + 0.15);
        // Rim light
        color += vec3(0.2, 0.5, 1.0) * pow(1.0 - max(dot(-rd, n), 0.0), 4.0) * 0.6;
    } else {
        color = vec3(0.02);
    }

    fragColor = vec4(color, 1.0);
}
