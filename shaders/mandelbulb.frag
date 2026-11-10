#version 410 core
// 3-D fractal renderer — ray-marched via distance estimator.
// Five modes selectable via u_fractal_3d:
//   0  Mandelbulb   (spherical power-n, classic 3-D fractal)
//   1  Mandelbox    (folding + scaling IFS — box-like structures)
//   2  Quaternion Julia  (4-D Julia set, 3-D cross-section)
//   3  3D Complex (Peng)  (commutative/associative 3-D complex algebra —
//      see "Extending complex number to spaces with 3, 4 or any number of
//      dimensions", Kuan Peng 2022. Distinct from the triplex power used by
//      Mandelbulb: multiplication here chains Euler's formula across two
//      orthogonal imaginary axes rather than a spherical-coordinate power.)
//   4  Kleinian Limit Set  (Möbius-transformation IFS — box fold + continuous
//      sphere inversion, same family as Mandelbox but tuned for the lace-like
//      circle-packing look of a Kleinian group's limit set rather than
//      Mandelbox's boxy structures. Shares u_mb_scale/u_mb_fold with
//      Mandelbox; u_kleinian_min_r2 is the one Kleinian-specific parameter.)

in  vec2 v_uv;
out vec4 fragColor;

uniform vec2  u_resolution;
uniform float u_time;
uniform float u_power;          // Mandelbulb: 2=sphere 8=classic >8=spiky
uniform int   u_max_iter;
uniform float u_zoom;
uniform vec2  u_julia_c;        // Quaternion Julia: c = (x, y, 0, 0)
uniform int   u_fractal_3d;     // 0=Mandelbulb 1=Mandelbox 2=QuatJulia 3=3D Complex (Peng) 4=Kleinian
uniform float u_mb_scale;       // Mandelbox / Kleinian scale  (default 2.0)
uniform float u_mb_fold;        // Mandelbox / Kleinian box half-size (default 1.0)
uniform float u_kleinian_min_r2; // Kleinian sphere-inversion radius^2 threshold
uniform sampler2D u_video_tex;
uniform sampler2D u_overlay_tex;
uniform float     u_overlay_blend;

// ════════════════════════════════════════════════════════════════════════════════
// DISTANCE ESTIMATORS
// ════════════════════════════════════════════════════════════════════════════════

// ── Mandelbulb (Inigo Quilez method) ─────────────────────────────────────────
float DE_mandelbulb(vec3 pos) {
    vec3  z  = pos;
    float dr = 1.0;
    float r  = 0.0;
    for (int i=0; i<64; i++) {
        r = length(z);
        if (r > 2.0) break;
        float theta = acos(clamp(z.z/r, -1.0, 1.0)) * u_power;
        float phi   = atan(z.y, z.x) * u_power;
        float zr    = pow(r, u_power);
        dr = pow(r, u_power-1.0)*u_power*dr + 1.0;
        z  = zr*vec3(sin(theta)*cos(phi), sin(theta)*sin(phi), cos(theta)) + pos;
    }
    return 0.5*log(r)*r/dr;
}

// ── Mandelbox ─────────────────────────────────────────────────────────────────
// Box + sphere folding IFS discovered by Tom Lowe (2010).
// Produces organic cubic/crystalline structures.
float DE_mandelbox(vec3 pos) {
    float scale = u_mb_scale;
    float fold  = u_mb_fold;
    vec3  z     = pos;
    float dr    = 1.0;

    for (int i=0; i<min(u_max_iter, 128); i++) {
        // Box fold: reflect z components back into [-fold, fold]
        z = clamp(z, -fold, fold)*2.0 - z;

        // Sphere fold: scale inside unit sphere / invert inside smaller sphere
        float r2 = dot(z,z);
        if      (r2 < 0.25)        { z *= 4.0;       dr *= 4.0;       }
        else if (r2 < 1.0)         { float k=1.0/r2; z *= k; dr *= k; }

        z  = z * scale + pos;
        dr = dr * abs(scale) + 1.0;

        if (length(z) > 100.0) break;
    }
    return (length(z) - abs(scale-1.0)*2.0 / abs(dr));
}

// ── Kleinian Limit Set ────────────────────────────────────────────────────────
// Same box-fold + scale-and-translate IFS as Mandelbox above, with one swap:
// a *continuous* sphere inversion (max(minR2/r2, 1.0)) in place of Mandelbox's
// two-tier fixed-radius ball fold. That single change is what turns the boxy
// Mandelbox look into a Kleinian group's lace-like circle-packing limit set —
// this is the well-known "Grandma's formula" construction from fractal-art
// circles (sphere inversion ⇄ generators of a Möbius transformation group).
float DE_kleinian(vec3 pos) {
    float scale = u_mb_scale;
    float box   = u_mb_fold;
    float minR2 = u_kleinian_min_r2;
    vec3  z     = pos;
    float dr    = 1.0;

    for (int i=0; i<min(u_max_iter, 128); i++) {
        // Box fold: reflect z components back into [-box, box]
        z = clamp(z, -box, box)*2.0 - z;

        // Continuous sphere inversion (the Möbius-transformation step)
        float r2 = dot(z,z);
        float k  = max(minR2/r2, 1.0);
        z  *= k;
        dr *= k;

        z  = z * scale + pos;
        dr = dr * abs(scale) + 1.0;

        if (length(z) > 100.0) break;
    }
    return (length(z) - abs(scale-1.0)*2.0 / abs(dr));
}

// ── Quaternion Julia ──────────────────────────────────────────────────────────
// 4-D Julia set via quaternion arithmetic, rendered as 3-D cross-section (w=0).
// c = (u_julia_c.x, u_julia_c.y, 0, 0)
// q^2 = (a²-b²-c²-d², 2ab, 2ac, 2ad)
vec4 qmul(vec4 a, vec4 b) {
    return vec4(
        a.x*b.x - a.y*b.y - a.z*b.z - a.w*b.w,
        a.x*b.y + a.y*b.x + a.z*b.w - a.w*b.z,
        a.x*b.z - a.y*b.w + a.z*b.x + a.w*b.y,
        a.x*b.w + a.y*b.z - a.z*b.y + a.w*b.x
    );
}
float DE_quatjulia(vec3 pos) {
    vec4 c = vec4(u_julia_c.x, u_julia_c.y, 0.0, 0.0);
    vec4 z = vec4(pos, 0.0);
    float md2 = 1.0;
    float mz2 = dot(z,z);
    for (int i=0; i<64; i++) {
        md2 *= 4.0 * mz2;
        z    = qmul(z, z) + c;
        mz2  = dot(z,z);
        if (mz2 > 4.0) break;
    }
    return 0.25 * sqrt(mz2/md2) * log(mz2);
}

// ── 3D Complex (Peng) ────────────────────────────────────────────────────────
// A 3-D complex number v = a + bi + cj, held as vec3(a,b,c). Unlike the
// triplex power used by Mandelbulb, this algebra's multiplication is
// commutative AND associative: it's built by converting to a trigonometric
// form — modulus |v| plus two independent angles theta (rotation in the h,i
// plane) and phi (elevation toward j) — then raising to a power multiplies
// the modulus and scales both angles, exactly like ordinary complex power
// but with one extra orthogonal imaginary axis.
//
//   |v|   = sqrt(a²+b²+c²)
//   phi   = asin(c / |v|)              -- elevation toward j
//   theta = atan2(b, a)                -- rotation in the h,i plane
//           (valid because cos(phi) is always taken positive, so it cancels
//           out of atan2(sin(theta)·cos(phi), cos(theta)·cos(phi)))
//   v^m   = |v|^m · (cos(m·theta)cos(m·phi),  sin(m·theta)cos(m·phi),  sin(m·phi))
vec3 complex3Pow(vec3 v, float m) {
    float r = length(v);
    if (r < 1e-6) return vec3(0.0);
    float phi   = asin(clamp(v.z / r, -1.0, 1.0));
    float theta = atan(v.y, v.x);
    float rm    = pow(r, m);
    float cmphi = cos(m * phi);
    return rm * vec3(cos(m * theta) * cmphi,
                      sin(m * theta) * cmphi,
                      sin(m * phi));
}

// Mandelbrot-style evaluation (every space point is its own c, z0=c),
// matching the convention DE_mandelbulb uses above. Reuses the same
// running-derivative DE approximation: valid here because v^m still scales
// the modulus by |v|^m each step, same as the triplex power does.
float DE_complex3d(vec3 pos) {
    vec3  z  = pos;
    float dr = 1.0;
    float r  = 0.0;
    for (int i=0; i<64; i++) {
        r = length(z);
        if (r > 2.0) break;
        dr = pow(r, u_power-1.0)*u_power*dr + 1.0;
        z  = complex3Pow(z, u_power) + pos;
    }
    return 0.5*log(r)*r/dr;
}

// ── Dispatch ─────────────────────────────────────────────────────────────────
float DE(vec3 p) {
    if (u_fractal_3d == 1) return DE_mandelbox(p);
    if (u_fractal_3d == 2) return DE_quatjulia(p);
    if (u_fractal_3d == 3) return DE_complex3d(p);
    if (u_fractal_3d == 4) return DE_kleinian(p);
    return DE_mandelbulb(p);
}

// ════════════════════════════════════════════════════════════════════════════════
// RAY MARCHER
// ════════════════════════════════════════════════════════════════════════════════
float march(vec3 ro, vec3 rd, out int steps) {
    float t = 0.0;
    steps = 0;
    for (int i=0; i<256; i++) {
        steps = i;
        float d = DE(ro + rd*t);
        if (d < 0.0005) return t;
        t += d;
        if (t > 8.0) break;
    }
    return -1.0;
}

// Surface normal via central differences
vec3 calcNormal(vec3 p) {
    vec2 e = vec2(0.001, 0.0);
    return normalize(vec3(
        DE(p+e.xyy)-DE(p-e.xyy),
        DE(p+e.yxy)-DE(p-e.yxy),
        DE(p+e.yyx)-DE(p-e.yyx)
    ));
}

// ── Ambient occlusion from step count ────────────────────────────────────────
float calcAO(vec3 p, vec3 n) {
    float ao = 0.0;
    for (int k=1; k<=5; k++) {
        float d = float(k)*0.08;
        ao += (d - DE(p + n*d)) / d;
    }
    return clamp(1.0 - ao*0.25, 0.0, 1.0);
}

// ════════════════════════════════════════════════════════════════════════════════
// MAIN
// ════════════════════════════════════════════════════════════════════════════════
void main() {
    vec2 uv = (v_uv - 0.5) * vec2(u_resolution.x/u_resolution.y, 1.0);

    // Orbit camera around the fractal
    float ang = u_time * 0.18;
    float dist = 2.4 / u_zoom;
    vec3 ro = vec3(sin(ang)*dist, sin(u_time*0.07)*0.6, cos(ang)*dist);
    vec3 target = vec3(0.0);
    vec3 fw = normalize(target - ro);
    vec3 ri = normalize(cross(fw, vec3(0.0, 1.0, 0.0)));
    vec3 up = cross(ri, fw);
    vec3 rd = normalize(uv.x*ri + uv.y*up + 1.8*fw);

    int steps;
    float t = march(ro, rd, steps);

    vec3 color;
    if (t > 0.0) {
        vec3 p = ro + rd*t;
        vec3 n = calcNormal(p);
        vec3 light1 = normalize(vec3( 1.0, 2.0,  1.5));
        vec3 light2 = normalize(vec3(-1.0, 0.5, -1.0));

        float diff  = max(dot(n, light1), 0.0);
        float back  = max(dot(n, light2), 0.0) * 0.2;
        float ao    = calcAO(p, n);
        float rim   = pow(1.0 - max(dot(-rd, n), 0.0), 4.0);

        // Map 3-D surface position to video texture UV
        // Use spherical projection for natural wrapping
        vec2 vidUV = vec2(
            atan(p.z, p.x) / 6.28318 + 0.5,
            acos(clamp(p.y / max(length(p), 0.001), -1.0, 1.0)) / 3.14159
        );
        vec3 videoColor = texture(u_video_tex, fract(vidUV)).rgb;

        // Shading
        vec3 base = videoColor * (diff + back) * ao;
        base += vec3(0.15, 0.40, 1.0) * rim * 0.7;   // electric blue rim
        base += videoColor * 0.08;                     // ambient
        color = base;
    } else {
        // Background: subtle depth fog using step count
        float fog = float(steps) / 256.0;
        color = vec3(0.01, 0.02, 0.04) + fog * vec3(0.02, 0.04, 0.08);
    }

    // ── Overlay video layer ───────────────────────────────────────────────────
    if (u_overlay_blend > 0.0) {
        vec3 overlay = texture(u_overlay_tex, v_uv).rgb;
        color = mix(color, overlay, u_overlay_blend);
    }

    fragColor = vec4(color, 1.0);
}
