#version 410 core
// MilkDrop preset transition blend — 10 transition modes
// Ported from MilkDrop.metal blend_fragment

in  vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_tex_a;    // outgoing preset
uniform sampler2D u_tex_b;    // incoming preset

uniform float u_blend;        // 0 = all A, 1 = all B
uniform int   u_blend_type;   // 0–9 (matches TransitionBlend enum)
uniform float u_time;

void main() {
    vec2  uv   = v_uv;
    float t    = u_blend;
    vec4  colA = texture(u_tex_a, uv);
    vec4  colB = texture(u_tex_b, uv);
    vec4  result;

    if (u_blend_type == 0) {
        // Zoom blend
        float zoom  = mix(1.0, 0.5, t);
        vec2  zUV   = (uv - 0.5) * zoom + 0.5;
        vec4  zB    = texture(u_tex_b, zUV);
        result = mix(colA, zB, smoothstep(0.3, 0.7, t));

    } else if (u_blend_type == 1) {
        // Side wipe
        result = (uv.x < t) ? colB : colA;

    } else if (u_blend_type == 2) {
        // Plasma mask
        float n    = sin(uv.x * 10.0 + u_time) * cos(uv.y * 8.0 - u_time * 0.7);
        float mask = step(n * 0.5 + 0.5, t);
        result = mix(colA, colB, mask);

    } else if (u_blend_type == 3) {
        // Cercle (expanding circle)
        float dist = length(uv - 0.5) * 2.0;
        float mask = step(dist, t * 1.414);
        result = mix(colA, colB, mask);

    } else if (u_blend_type == 4) {
        // Checkerboard
        float cx      = floor(uv.x * 8.0);
        float cy      = floor(uv.y * 8.0);
        float checker = mod(cx + cy, 2.0);
        float edge    = checker < 0.5 ? t * 2.0 : t * 2.0 - 1.0;
        result = mix(colA, colB, clamp(edge, 0.0, 1.0));

    } else if (u_blend_type == 5) {
        // Stars (radial burst)
        vec2  d    = uv - 0.5;
        float ang  = atan(d.y, d.x);
        float star = sin(ang * 8.0) * 0.1 + 0.9;
        float dist2 = length(d) * 2.0 / star;
        result = mix(colA, colB, step(dist2, t));

    } else if (u_blend_type == 6) {
        // Bezier warp morph
        float mt  = 1.0 - t;
        float mt2 = mt * mt;
        float t2  = t * t;
        vec2 ctrl1 = vec2(0.5 + sin(u_time * 0.7) * 0.3,  0.2 + cos(u_time * 0.5) * 0.2);
        vec2 ctrl2 = vec2(0.5 + cos(u_time * 0.6) * 0.3,  0.8 + sin(u_time * 0.4) * 0.2);
        vec2 warpOffset = (ctrl1 * 3.0 * mt2 * t + ctrl2 * 3.0 * mt * t2) - uv;
        vec2 warpedUV   = clamp(uv + warpOffset * t * (1.0 - t) * 2.0, 0.0, 1.0);
        vec4 warpedA    = texture(u_tex_a, warpedUV);
        result = mix(warpedA, colB, smoothstep(0.2, 0.8, t));

    } else if (u_blend_type == 7) {
        // Mesh morph (grid-based warp)
        vec2 cell     = floor(uv * 8.0) / 8.0;
        vec2 morphOff = vec2(sin(cell.x * 6.28 + cell.y * 4.19 + u_time) * 0.08,
                             cos(cell.x * 5.13 + cell.y * 7.31 + u_time * 0.8) * 0.08);
        vec2 uvA = clamp(uv + morphOff * (1.0 - t), 0.0, 1.0);
        vec2 uvB = clamp(uv - morphOff * t,          0.0, 1.0);
        result = mix(texture(u_tex_a, uvA), texture(u_tex_b, uvB), smoothstep(0.1, 0.9, t));

    } else if (u_blend_type == 8) {
        // Fractal dissolve (Mandelbrot mask)
        vec2  fc   = (uv - 0.5) * 3.5;
        vec2  z2   = vec2(0.0);
        int   iter = 0;
        for (int i = 0; i < 24; i++) {
            z2 = vec2(z2.x * z2.x - z2.y * z2.y + fc.x,
                      2.0 * z2.x * z2.y + fc.y);
            if (dot(z2, z2) > 4.0) break;
            iter++;
        }
        float mask = float(iter) / 24.0;
        result = mix(colA, colB, step(mask, t));

    } else if (u_blend_type == 9) {
        // Spiral twist
        vec2  p      = uv * 2.0 - 1.0;
        float radius = length(p);
        float ang2   = atan(p.y, p.x);
        float twist  = sin(radius * 4.0 * 3.14159 - u_time * 2.0) * t * 1.5;
        vec2  twUV   = vec2(0.5 + 0.5 * cos(ang2 + twist) * radius,
                            0.5 + 0.5 * sin(ang2 + twist) * radius);
        twUV = clamp(twUV, 0.0, 1.0);
        result = mix(texture(u_tex_a, twUV), colB, smoothstep(0.15, 0.85, t));

    } else {
        // Simple cross-fade (fallback)
        result = mix(colA, colB, t);
    }

    fragColor = result;
}
