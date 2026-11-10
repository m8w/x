#version 410 core
// ── Quaternion Feedback ────────────────────────────────────────────────────────
// A post-process feedback-trail effect: each frame, the previous feedback
// frame is re-sampled at a UV offset driven by a 3-D quaternion rotation,
// then blended with the fresh fractal render and written back — a classic
// "trail" effect, except the trail's swirl direction comes from true
// non-commutative 3-D rotation instead of a flat 2-D spin.
//
// Two independent rotation *rates* ("real" and "imaginary," named after the
// idea that inspired this — evocative naming, not a physics claim) are
// composed as two quaternions about different axes. Because quaternion
// multiplication doesn't commute, that composition keeps drifting instead
// of settling into a repeating circular spin, which is what gives the
// trail its braided, self-crossing look.
//
// Unlike Buddhabrot/Flame this is a stateless single-pass effect (no
// geometry shader, no two-step decay+accumulate) — cheap enough to layer on
// top of any fractal mode, including Buddhabrot/Flame's own output.

in  vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_fbo_tex;    // this frame's fresh render
uniform sampler2D u_prev_tex;   // previous feedback frame
uniform vec2      u_resolution;
uniform float     u_time;

uniform float u_amount;      // 0 = fractal only   1 = feedback trail dominates
uniform float u_decay;       // per-frame trail fade
uniform float u_warp;        // UV-offset magnitude from the rotation
uniform float u_real_rate;   // "real time" rotation rate (rad/s)
uniform float u_imag_rate;   // "imaginary time" rotation rate (rad/s)
uniform vec3  u_axis;        // i,j,k axis weights (normalised below)

// ── Quaternion helpers (GLSL layout: xyz = vector part, w = scalar part) ──────
vec4 qFromAxisAngle(vec3 axis, float angle) {
    vec3 a = normalize(axis + 1e-6);   // guard the degenerate all-zero axis
    float s = sin(angle * 0.5);
    return vec4(a * s, cos(angle * 0.5));
}
vec4 qMul(vec4 a, vec4 b) {
    return vec4(a.w*b.xyz + b.w*a.xyz + cross(a.xyz, b.xyz),
                a.w*b.w - dot(a.xyz, b.xyz));
}
vec4 qConj(vec4 q) { return vec4(-q.xyz, q.w); }
vec3 qRotate(vec4 q, vec3 v) {
    return qMul(qMul(q, vec4(v, 0.0)), qConj(q)).xyz;
}

void main() {
    // Two independently-rated rotations about related-but-different axes —
    // the cyclic permutation keeps them from sharing an axis, so composing
    // them is genuinely 3-D (not reducible to one combined 2-D spin).
    vec4 qReal = qFromAxisAngle(u_axis,       u_time * u_real_rate);
    vec4 qImag = qFromAxisAngle(u_axis.yzx,   u_time * u_imag_rate);
    vec4 q     = qMul(qReal, qImag);

    vec3 probe  = qRotate(q, vec3(1.0, 0.0, 0.0));
    vec2 warpUV = v_uv + probe.xy * u_warp;

    vec3 fresh = texture(u_fbo_tex,  clamp(v_uv,   0.0, 1.0)).rgb;
    vec3 trail = texture(u_prev_tex, clamp(warpUV, 0.0, 1.0)).rgb * u_decay;

    // Trails add on top of the fresh render rather than replacing it, so
    // "Amount" at 1.0 still shows the current fractal, just buried under a
    // dense braided trail instead of blanked to black.
    vec3 color = mix(fresh, max(fresh, trail), clamp(u_amount, 0.0, 1.0));
    fragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
