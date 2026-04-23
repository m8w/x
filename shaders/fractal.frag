#version 410 core

in  vec2 v_uv;
out vec4 fragColor;

// ── Uniforms ──────────────────────────────────────────────────────────────────
uniform vec2  u_resolution;
uniform float u_time;
uniform float u_blend_mandelbrot;
uniform float u_blend_julia;
uniform float u_blend_mandelbulb;
uniform float u_blend_euclidean;
uniform float u_blend_diff;        // de Jong differential flow field blend
uniform vec2  u_julia_c;
uniform float u_power;
uniform int   u_max_iter;
uniform float u_bailout;
uniform float u_zoom;
uniform vec2  u_offset;
uniform int   u_formula;           // formula A  (0–21)
uniform int   u_formula_b;         // formula B  (blend crossfades A→B)
uniform float u_formula_blend;     // 0=pure A  1=pure B
uniform float u_formula_param;     // free extra parameter (rotation speed, warp strength…)
uniform float u_pixel_weight;      // injects screen position into seed
uniform float u_geo_warp;
uniform int   u_geo_shape;
uniform int   u_geo_sides;
uniform float u_geo_radius;
uniform float u_geo_rotation;
uniform bool  u_geo_tile;
uniform int   u_geo_mirror;        // 0=none 1=X 2=Y 3=XY
uniform int   u_geo_kaleid;        // 0=off  N=number of kaleidoscope segments (2–16)
uniform int   u_layer_count;       // 1–4: spatial layer repetition
uniform float u_layer_offset;      // gap between layers
uniform sampler2D u_video_tex;
uniform sampler2D u_overlay_tex;   // second video layer
uniform float     u_overlay_blend; // 0=fractal only  1=overlay only  0.5=50/50

// ── Video filters ─────────────────────────────────────────────────────────────
// Applied independently to the primary video and the overlay stream.
// Color filters (IDs 0–11) work on any stream.
// Spatial filters (IDs 12–18) only applied to overlay (has screen UV).
uniform int   u_vid_filter;      // primary video filter ID
uniform float u_vid_fa;          // filter param A
uniform float u_vid_fb;          // filter param B
uniform int   u_ovr_filter;      // overlay filter ID
uniform float u_ovr_fa;          // overlay filter param A
uniform float u_ovr_fb;          // overlay filter param B
uniform vec2  u_overlay_size;    // overlay texture size (pixels) for spatial filters

// ── Stream blend mode ─────────────────────────────────────────────────────────
// How the fractal+primary composite blends with the overlay stream.
// 0=Normal  1=Multiply  2=Screen  3=Overlay  4=SoftLight  5=HardLight
// 6=Difference  7=Exclusion  8=ColorDodge  9=ColorBurn  10=Darken  11=Lighten  12=Addition
uniform int u_stream_blend_mode;

// ── Chaos Effects ─────────────────────────────────────────────────────────────
uniform int   u_chaos_mode;     // 0=off  1=turbulence  2=logistic  3=henon  4=shred
uniform float u_chaos_strength; // overall warp amplitude (0–1)
uniform float u_chaos_scale;    // spatial frequency / map scale (0.5–8)
uniform float u_chaos_speed;    // time modulation rate (0–3)

// ── Color Synthesizer ─────────────────────────────────────────────────────────
uniform bool  u_cs_enabled;
uniform vec3  u_cs_hsl;         // primary HSL (hue 0-1 wrapping, sat 0-1, lum 0-1)
uniform vec3  u_cs_hsl_alt;     // alternate HSL
uniform float u_cs_alt_blend;   // 0=primary  1=alt  (oscillates)
uniform int   u_cs_mode;        // 0=replace  1=multiply  2=screen
uniform float u_cs_hue_spread;  // hue range spread across escape value
uniform float u_cs_lum_spread;  // lum range spread across escape value

// ════════════════════════════════════════════════════════════════════════════════
// COMPLEX NUMBER LIBRARY
// ════════════════════════════════════════════════════════════════════════════════
vec2 cmul (vec2 a, vec2 b) { return vec2(a.x*b.x-a.y*b.y, a.x*b.y+a.y*b.x); }
vec2 csqr (vec2 z)         { return vec2(z.x*z.x-z.y*z.y, 2.0*z.x*z.y); }
vec2 ccube(vec2 z)         { return cmul(csqr(z), z); }
vec2 cconj(vec2 z)         { return vec2(z.x, -z.y); }
float cabs2(vec2 z)        { return dot(z,z); }
vec2 cinv  (vec2 z)        { return cconj(z)/dot(z,z); }
vec2 cdiv  (vec2 a, vec2 b){ return cmul(a, cinv(b)); }

vec2 cexp(vec2 z) { return exp(z.x)*vec2(cos(z.y), sin(z.y)); }
vec2 clog(vec2 z) { return vec2(log(max(length(z),1e-10)), atan(z.y,z.x)); }

vec2 cpow_r(vec2 z, float n) {
    if (length(z) < 1e-10) return vec2(0.0);
    float th = atan(z.y, z.x)*n;
    return pow(length(z), n)*vec2(cos(th), sin(th));
}
vec2 cpow_c(vec2 z, vec2 w) { return cexp(cmul(w, clog(z))); }

vec2 csin (vec2 z) { return vec2( sin(z.x)*cosh(z.y),  cos(z.x)*sinh(z.y)); }
vec2 ccos (vec2 z) { return vec2( cos(z.x)*cosh(z.y), -sin(z.x)*sinh(z.y)); }
vec2 ctan (vec2 z) { return cdiv(csin(z), ccos(z)); }

vec2 csinh(vec2 z) { return vec2(sinh(z.x)*cos(z.y), cosh(z.x)*sin(z.y)); }
vec2 ccosh(vec2 z) { return vec2(cosh(z.x)*cos(z.y), sinh(z.x)*sin(z.y)); }
vec2 ctanh(vec2 z) { return cdiv(csinh(z), ccosh(z)); }

vec2 csqrt(vec2 z) {
    float r=length(z); float t=atan(z.y,z.x)*0.5;
    return sqrt(r)*vec2(cos(t),sin(t));
}

// ════════════════════════════════════════════════════════════════════════════════
// SDF GEOMETRY LIBRARY
// ════════════════════════════════════════════════════════════════════════════════
float sdf_circle (vec2 p, float r) { return length(p)-r; }
float sdf_polygon(vec2 p, int n, float r) {
    float a=atan(p.y,p.x)+u_geo_rotation, s=6.28318/float(n);
    a=abs(mod(a,s)-s*0.5);
    return length(p)*cos(a)-r;
}
float sdf_star(vec2 p, int n, float r) {
    float a=atan(p.y,p.x)+u_geo_rotation, s=6.28318/float(n*2);
    a=abs(mod(a,2.0*s)-s);
    return length(p)*cos(a)-mix(r,r*0.45,step(s*0.5,a));
}
float sdf_grid(vec2 p, float cell) {
    vec2 q=mod(p,cell)-cell*0.5;
    return min(abs(q.x),abs(q.y))-cell*0.05;
}
float sdf_eval(vec2 p) {
    vec2 q = u_geo_tile ? mod(p*2.0,2.0)-1.0 : p;
    float r=u_geo_radius;
    if (u_geo_shape==0) return sdf_circle (q,r);
    if (u_geo_shape==1) return sdf_polygon(q,u_geo_sides,r);
    if (u_geo_shape==2) return sdf_star   (q,u_geo_sides,r);
                        return sdf_grid   (q,r);
}
vec2 sdf_grad(vec2 p) {
    const float h=0.0015;
    float d=sdf_eval(p);
    return vec2(sdf_eval(p+vec2(h,0.0))-d, sdf_eval(p+vec2(0.0,h))-d)/h;
}

// ════════════════════════════════════════════════════════════════════════════════
// POLAR ↔ CARTESIAN  HELPERS
//   These let formulas reason in magnitude/angle space and convert back.
//   polar2cart(r, theta) → (r·cos θ, r·sin θ)
//   cart2polar(z)        → (|z|, atan2(z.y, z.x))
// ════════════════════════════════════════════════════════════════════════════════
vec2 polar2cart(float r, float theta) {
    return r * vec2(cos(theta), sin(theta));
}
vec2 cart2polar(vec2 z) {
    return vec2(length(z), atan(z.y, z.x));
}

// ════════════════════════════════════════════════════════════════════════════════
// FORMULA EVALUATOR  (IDs 0 – 21)
// Applies one iteration step for the given formula ID.
// Newton variants (f==8, f==19) return a Newton-step; convergence is checked
// by the caller (iterate()) via the root-distance trap.
//
// u_formula_param — free extra parameter used by:
//   f=18  Time-spiral : rotation speed (rad/s)  default 1.0
//   f=21  Polar warp  : polar-to-Cartesian warp blend  0=flat  1=full
// ════════════════════════════════════════════════════════════════════════════════
vec2 eval_formula(int f, vec2 z, vec2 z_prev, vec2 seed) {
    // ── Original 11 (IDs 0–10) ────────────────────────────────────────────────
    if (f == 0) return csqr(z) + seed;
    if (f == 1) return csin(z) + seed;
    if (f == 2) return cexp(z) + seed;
    if (f == 3) return ccos(z) + seed;
    if (f == 4) return csinh(z) + seed;
    if (f == 5) return ccosh(z) + seed;
    if (f == 6) return csqr(vec2(abs(z.x), abs(z.y))) + seed;  // Burning Ship
    if (f == 7) return csqr(cconj(z)) + seed;                  // Tricorn
    if (f == 8) {                                               // Newton z³−1
        vec2 den = 3.0 * csqr(z);
        if (cabs2(den) < 1e-12) return z;
        return z - cdiv(ccube(z) - vec2(1.0, 0.0), den);
    }
    if (f == 9) return csqr(z) + vec2(seed.x, 0.0) + seed.y * z_prev; // Phoenix
    if (f == 10) return cpow_r(z, u_power) + seed;             // z^n + c

    // ── New formulas (IDs 11–21) ──────────────────────────────────────────────

    // 11 — Tangent: complex poles create flame-like filigree
    if (f == 11) return ctan(z) + seed;

    // 12 — z·exp(z): exponential-polynomial, produces spiral galaxy arms
    if (f == 12) return cmul(z, cexp(z)) + seed;

    // 13 — Celtic: fold only Re(z²) after squaring (distinct from Burning Ship)
    if (f == 13) {
        vec2 z2 = csqr(z);
        return vec2(abs(z2.x), z2.y) + seed;
    }

    // 14 — Magnet I: ((z²+c−1)/(2z+c−2))²   classic magnetic attractor
    if (f == 14) {
        vec2 num = csqr(z) + seed - vec2(1.0, 0.0);
        vec2 den = 2.0*z + seed - vec2(2.0, 0.0);
        if (cabs2(den) < 1e-10) return z;
        return csqr(cdiv(num, den));
    }

    // 15 — z^z + c: complex self-power (z raised to itself)
    if (f == 15) {
        if (length(z) < 1e-5) return seed;
        return cpow_c(z, z) + seed;
    }

    // 16 — Manowar: z² + z_{n-1} + c  (direct two-step memory)
    if (f == 16) return csqr(z) + z_prev + seed;

    // 17 — Perpendicular Burning Ship: fold imaginary axis only
    if (f == 17) return csqr(vec2(z.x, abs(z.y))) + seed;

    // 18 — Time-spiral: rotate z² by u_formula_param·u_time radians each step
    if (f == 18) {
        float ang = u_formula_param * u_time;
        return cmul(csqr(z), polar2cart(1.0, ang)) + seed;
    }

    // 19 — Cubic + linear: z³ + z + c  (richer basin structure than z³+c)
    if (f == 19) return ccube(z) + z + seed;

    // 20 — Cosh-conjugate: cosh(conj(z)) + c  (conjugate-reflection symmetry)
    if (f == 20) return ccosh(cconj(z)) + seed;

    // 21 — Polar↔Cartesian warp:
    //   Convert z to polar, scale angle by u_formula_param, convert back,
    //   then square and add c.  At param=1 this is ordinary z²+c; as param
    //   deviates the angular step twists the orbit path.
    if (f == 21) {
        vec2 pol = cart2polar(z);           // (r, θ)
        float r  = pol.x;
        float th = pol.y * u_formula_param; // twist angle by param
        vec2 zw  = polar2cart(r, th);       // back to Cartesian
        return csqr(zw) + seed;
    }

    // ── Mandelbulber 2 extended set (IDs 22–35) ───────────────────────────────

    // 22 — Buffalo: abs on BOTH Re and Im of z² before adding c
    //   Distinct from Burning Ship (which folds before squaring) and Celtic
    //   (which only folds Re).  Produces symmetric four-quadrant structures.
    if (f == 22) {
        vec2 z2 = csqr(z);
        return vec2(abs(z2.x), abs(z2.y)) + seed;
    }

    // 23 — Perpendicular Celtic: fold Im(z²) only, leave Re(z²) unchanged.
    //   Counterpart to Celtic (f=13) which folds Re; produces distinct
    //   vertical-axis symmetric filaments.
    if (f == 23) {
        vec2 z2 = csqr(z);
        return vec2(z2.x, abs(z2.y)) + seed;
    }

    // 24 — tanh(z) + c: complex hyperbolic tangent.
    //   Produces enclosed bounded regions with smooth gradient halos;
    //   related to ctan (f=11) but with hyperbolic instead of circular poles.
    if (f == 24) return ctanh(z) + seed;

    // 25 — Nova (Newton z³−1 + c perturbation):
    //   Standard Newton step plus an additive c term.  Keeps the three-root
    //   convergence basins of Newton while the c perturbation distorts them.
    if (f == 25) {
        vec2 den = 3.0 * csqr(z);
        if (cabs2(den) < 1e-12) return z + seed;
        return z - cdiv(ccube(z) - vec2(1.0, 0.0), den) + seed;
    }

    // 26 — Lambda: z(1−z)·c
    //   Completely different topology — fixed points at 0 and 1; produces
    //   Douady rabbit / airplane / basilica families depending on c.
    if (f == 26) return cmul(cmul(z, vec2(1.0, 0.0) - z), seed);

    // 27 — Barnsley 1: IFS branching on sign of Re(z·conj(c))
    //   Two affine branches selected per iteration by the sign of the inner
    //   product; generates fern-like IFS attractors in the filled Julia set.
    if (f == 27) {
        if (dot(z, seed) >= 0.0)
            return cmul(z - vec2(1.0, 0.0), seed);
        else
            return cmul(z + vec2(1.0, 0.0), seed);
    }

    // 28 — SimFp: sinh(z) + z² + c
    //   Hybrid hyperbolic-polynomial; the two terms compete, producing
    //   complex basins that combine lobe structures from both functions.
    if (f == 28) return csinh(z) + csqr(z) + seed;

    // 29 — Ikenaga: z³ + (c−1)z − c
    //   Cubic with a linear (c−1)z perturbation; richer basin structure
    //   than plain z³+c, inspired by the Ikenaga fractal from Mandelbulber.
    if (f == 29) return ccube(z) + cmul(seed - vec2(1.0, 0.0), z) - seed;

    // 30 — Rudy: z² + c/z
    //   Rational map — the inverse term c/z creates ring-shaped structures
    //   and a pole at the origin that distorts nearby orbits dramatically.
    if (f == 30) {
        if (cabs2(z) < 1e-10) return seed;
        return csqr(z) + cdiv(seed, z);
    }

    // 31 — Magnet II: ((z³+3(c−1)z+(c−1)(c−2))/(3z²+3(c−2)z+(c−1)(c−2)+1))²
    //   Second-order magnetic attractor rational map.  Produces complex
    //   interlocking domains; paired with Magnet I (f=14) for A↔B blend.
    if (f == 31) {
        vec2 c1  = seed - vec2(1.0, 0.0);          // c−1
        vec2 c2  = seed - vec2(2.0, 0.0);          // c−2
        vec2 c12 = cmul(c1, c2);                    // (c−1)(c−2)
        vec2 num = ccube(z) + 3.0*cmul(c1, z) + c12;
        vec2 den = 3.0*csqr(z) + 3.0*cmul(c2, z) + c12 + vec2(1.0, 0.0);
        if (cabs2(den) < 1e-10) return z;
        return csqr(cdiv(num, den));
    }

    // 32 — z⁴ + c: fourth power Mandelbrot
    //   Three-fold symmetry axis; produces the classic 4-lobed Mandelbrot
    //   shape.  Richer fine structure than z² at the same iteration count.
    if (f == 32) return cpow_r(z, 4.0) + seed;

    // 33 — Glynn: z^1.5 + c (fractional power)
    //   Non-integer exponent via polar form; produces asymmetric branching
    //   dendrites — the classic "Glynn fractal" shape.
    if (f == 33) return cpow_r(z, 1.5) + seed;

    // 34 — Mandelbar Celtic: conjugate z before Celtic fold
    //   Apply conjugation first, then fold only Re(conj(z)²).  Combines
    //   the three-fold Mandelbar symmetry with Celtic's one-sided fold.
    if (f == 34) {
        vec2 z2 = csqr(cconj(z));
        return vec2(abs(z2.x), z2.y) + seed;
    }

    // 35 — Magnitude-coupled: z² · sin(|z|) + c
    //   Scales each iteration by the sine of the orbit radius, injecting
    //   concentric ring modulation into the escape path.
    if (f == 35) return cmul(csqr(z), vec2(sin(length(z)), 0.0)) + seed;

    return csqr(z) + seed;  // fallback
}

// ════════════════════════════════════════════════════════════════════════════════
// MANDELBULB 2-D SLICE
// ════════════════════════════════════════════════════════════════════════════════
float mandelbulb_slice(vec2 c) {
    vec2 z=c;
    for (int i=0; i<u_max_iter; i++) {
        float zr=pow(length(z), u_power);
        float ang=atan(z.y,z.x)*u_power;
        z=zr*vec2(cos(ang),sin(ang))+c;
        if (length(z)>u_bailout) return float(i)/float(u_max_iter);
    }
    return 0.0;
}

// ════════════════════════════════════════════════════════════════════════════════
// DE JONG DIFFERENTIAL FLOW FIELD  (5th blend mode)
// Integrates a de Jong attractor ODE from each pixel.
// Parameters driven by julia_c and power so they respond to the same UI sliders.
// ════════════════════════════════════════════════════════════════════════════════
float diff_flow(vec2 p) {
    float a  = u_julia_c.x * 3.14159 + 1.4;
    float b  = u_julia_c.y * 3.14159 + 1.56;
    float c2 = clamp(u_power * 0.2, 0.3, 3.0);
    float d  = 1.5 + 0.3 * sin(u_time * 0.07);
    vec2  z  = p;
    float trap = 1e6;
    for (int i = 0; i < u_max_iter; i++) {
        vec2 nz;
        nz.x = sin(a * z.y) - cos(b * z.x);
        nz.y = sin(c2 * z.x) - cos(d * z.y);
        trap = min(trap, length(nz - p));
        if (length(nz) > u_bailout * 2.0)
            return float(i) / float(u_max_iter);
        z = nz;
    }
    return clamp(1.0 - trap * 0.5, 0.0, 1.0);
}

// ════════════════════════════════════════════════════════════════════════════════
// COMBINED FRACTAL + SDF ITERATION
// Formula blend: u_formula_blend crossfades between formula A and formula B.
// Pixel weight:  u_pixel_weight adds the world-space pixel coord to the seed,
//                making screen position a live variable inside the equation.
// ════════════════════════════════════════════════════════════════════════════════

const vec2 kRoot0 = vec2( 1.000,  0.000);
const vec2 kRoot1 = vec2(-0.500,  0.866);
const vec2 kRoot2 = vec2(-0.500, -0.866);

float iterate(vec2 z_init, vec2 seed) {
    vec2  z      = z_init;
    vec2  z_prev = z;
    float trap   = 1e6;

    // Newton convergence coloring: determined by whichever formula dominates
    bool newton_a = (u_formula   == 8);
    bool newton_b = (u_formula_b == 8);

    for (int i = 0; i < u_max_iter; i++) {

        // ── Formula A × B cross-blend ─────────────────────────────────────────
        vec2 z_a = eval_formula(u_formula,   z, z_prev, seed);
        vec2 z_b = eval_formula(u_formula_b, z, z_prev, seed);

        vec2 z_temp = z;
        z = (u_formula_blend > 0.999) ? z_b
          : (u_formula_blend < 0.001) ? z_a
          : mix(z_a, z_b, u_formula_blend);
        z_prev = z_temp;

        // ── SDF orbit trap + warp ─────────────────────────────────────────────
        if (u_blend_euclidean > 0.001) {
            float d = sdf_eval(z);
            trap = min(trap, abs(d));
            if (u_geo_warp > 0.001) {
                vec2 g = sdf_grad(z);
                z -= normalize(g) * sign(d) * u_geo_warp * 0.035;
            }
        }

        // ── Escape / convergence ──────────────────────────────────────────────
        bool is_newton = (u_formula_blend < 0.5) ? newton_a : newton_b;
        if (is_newton) {
            float nearest = min(length(z-kRoot0), min(length(z-kRoot1), length(z-kRoot2)));
            trap = min(trap, nearest);
            if (nearest < 0.001) return float(i) / float(u_max_iter);
        } else {
            float r2 = dot(z,z);
            if (r2 > u_bailout*u_bailout) {
                float escape_t = float(i)/float(u_max_iter)
                              - log2(log2(max(r2,1.0001)))/float(u_max_iter);
                float trap_c = clamp(1.0-trap*(2.0+u_geo_warp*10.0), 0.0, 1.0);
                return mix(escape_t, trap_c, clamp(u_blend_euclidean*1.8, 0.0, 1.0));
            }
        }
    }
    return clamp(1.0-trap*2.0, 0.0, 1.0) * u_blend_euclidean;
}

// ════════════════════════════════════════════════════════════════════════════════
// MIRROR / KALEIDOSCOPE  — fold the complex plane before iteration
// ════════════════════════════════════════════════════════════════════════════════

// Mirror: fold one or both axes so the fractal is symmetrised
vec2 apply_mirror(vec2 p) {
    if (u_geo_mirror == 1) p.x = abs(p.x);          // X mirror
    if (u_geo_mirror == 2) p.y = abs(p.y);          // Y mirror
    if (u_geo_mirror == 3) { p.x = abs(p.x); p.y = abs(p.y); }  // XY
    return p;
}

// Kaleidoscope: repeat N wedges around the origin.
// Works by snapping the angle to the first (2π/N) sector then mirroring
// within it — identical to the classic kscope trick used in demoscene shaders.
vec2 apply_kaleid(vec2 p) {
    if (u_geo_kaleid < 2) return p;
    float n   = float(u_geo_kaleid);
    float ang = atan(p.y, p.x);
    float r   = length(p);
    float seg = 6.28318530718 / n;          // 2π / N
    ang = mod(ang, seg);                    // snap to first wedge
    ang = min(ang, seg - ang);             // mirror within wedge
    return r * vec2(cos(ang), sin(ang));
}

// ════════════════════════════════════════════════════════════════════════════════
// COLOUR PALETTE  (IQ cosine palette)
// ════════════════════════════════════════════════════════════════════════════════
vec3 palette(float t) {
    return vec3(0.5)+vec3(0.5)*cos(6.28318*(vec3(1.0,1.0,0.5)*t+vec3(0.8,0.9,0.3)));
}

// ── HSL → RGB (compact version) ───────────────────────────────────────────────
vec3 hsl2rgb(vec3 c) {
    vec3 rgb = clamp(abs(mod(c.x*6.0+vec3(0.0,4.0,2.0),6.0)-3.0)-1.0, 0.0, 1.0);
    return c.z + c.y*(rgb-0.5)*(1.0-abs(2.0*c.z-1.0));
}

// ── RGB → HSL ─────────────────────────────────────────────────────────────────
vec3 rgb2hsl(vec3 c) {
    float maxC = max(max(c.r,c.g),c.b);
    float minC = min(min(c.r,c.g),c.b);
    float d    = maxC - minC;
    float l    = (maxC + minC) * 0.5;
    float s    = (l < 0.5) ? d/(maxC+minC+1e-6) : d/(2.0-maxC-minC+1e-6);
    float h = 0.0;
    if (d > 1e-6) {
        if (maxC == c.r) h = (c.g-c.b)/d + (c.g<c.b ? 6.0 : 0.0);
        else if (maxC == c.g) h = (c.b-c.r)/d + 2.0;
        else                  h = (c.r-c.g)/d + 4.0;
        h /= 6.0;
    }
    return vec3(h, s, l);
}

// ── Synth palette: derive an HSL colour for escape value t ────────────────────
vec3 synthPalette(float t, vec3 hsl) {
    float h = fract(hsl.x + t * u_cs_hue_spread);
    float l = clamp(hsl.z + (t - 0.5) * u_cs_lum_spread, 0.0, 1.0);
    return hsl2rgb(vec3(h, hsl.y, l));
}

// ════════════════════════════════════════════════════════════════════════════════
// CHAOS DOMAIN WARPS
// Applied as a pre-iteration UV warp — distorts the complex plane before any
// fractal evaluation so the chaotic geometry is baked into the fractal structure
// rather than applied as a post-process.
//
// Mode 1 — Turbulence: two-level fBm domain warp (Quilez-style).
//           Produces smooth, continuously-flowing chaotic streams.
// Mode 2 — Logistic:  logistic map r·x·(1-x) iterated 8× in polar coordinates.
//           As r→4 the orbit bifurcates to full chaos; drives a rotation warp.
// Mode 3 — Hénon:    6 iterations of the Hénon attractor (a=1.4, b=0.3)
//           applied as a displacement from the attractor trajectory.
// Mode 4 — Shred:    time-varying scanline horizontal drift.
//           Multi-frequency sine sum gives irregular tape-shred / signal-loss.
// ════════════════════════════════════════════════════════════════════════════════

// Smooth value noise (no texture lookup — deterministic hash)
float _h(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.545); }
float _n(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f*f*(3.0 - 2.0*f);
    return mix(mix(_h(i), _h(i+vec2(1,0)), f.x),
               mix(_h(i+vec2(0,1)), _h(i+vec2(1,1)), f.x), f.y);
}

vec2 apply_chaos(vec2 p, float t) {
    if (u_chaos_mode == 0) return p;

    float st = t * u_chaos_speed;
    float sc = u_chaos_scale;
    float sw = u_chaos_strength;

    // ── Mode 1: Turbulence — two-level fBm warp ───────────────────────────────
    if (u_chaos_mode == 1) {
        // First warp layer
        vec2 q = vec2(_n(p*sc + st*0.11        ) - 0.5,
                      _n(p*sc + vec2(3.7, 5.1) + st*0.09) - 0.5);
        // Second layer warped by the first (increases folding complexity)
        q = vec2(_n(p*sc + q + st*0.06        ) - 0.5,
                 _n(p*sc + q + vec2(1.7, 9.2) + st*0.07) - 0.5);
        return p + q * sw * 0.6;
    }

    // ── Mode 2: Logistic — polar rotation driven by logistic orbit ───────────
    if (u_chaos_mode == 2) {
        // r sweeps 3.57 (onset of chaos) → 4.0 (full chaos) with strength
        float r = 3.57 + sw * 0.43;
        // Two independent logistic seeds derived from polar coords
        float lx = fract(length(p) * sc * 0.5 + 0.13);
        float ly = fract(atan(p.y, p.x) / 6.28318 + 0.5 + st * 0.04);
        for (int i = 0; i < 8; i++) {
            lx = r * lx * (1.0 - lx);
            ly = r * ly * (1.0 - ly);
        }
        // Map combined orbit to a rotation angle
        float angle = (lx + ly - 1.0) * 3.14159 * sw;
        float cr = cos(angle), sr = sin(angle);
        return vec2(cr*p.x - sr*p.y, sr*p.x + cr*p.y);
    }

    // ── Mode 3: Hénon — 2-D chaotic attractor warp ───────────────────────────
    if (u_chaos_mode == 3) {
        const float a = 1.4, b = 0.3;
        vec2 h = p * sc;
        // 6 iterations — enough to move the point into the attractor basin
        for (int i = 0; i < 6; i++) {
            h = vec2(1.0 - a*h.x*h.x + h.y, b*h.x);
        }
        // Clamp attractor displacement to prevent extreme values at boundaries
        return p + clamp(h, -3.0, 3.0) * sw * 0.04;
    }

    // ── Mode 4: Shred — multi-freq scanline horizontal drift ─────────────────
    if (u_chaos_mode == 4) {
        float row = p.y * sc * 8.0;
        float d = sin(st * 3.7  + row * 15.0) * 0.50
                + sin(st * 7.1  + row *  5.3) * 0.25
                + sin(st * 1.3  + row * 31.0) * 0.25;
        return p + vec2(d * sw * 0.25, 0.0);
    }

    // ── Mode 5: Lorenz — slice of the Lorenz strange attractor ───────────────
    // Integrates the Lorenz ODE (σ=10, ρ=28, β=8/3) from the pixel position
    // for a few Euler steps.  The (x,y) displacement is used as a warp vector.
    // Produces the characteristic butterfly-wing flow fields.
    if (u_chaos_mode == 5) {
        const float sigma = 10.0, rho = 28.0, beta = 2.667;
        float dt = 0.008 * sw;
        float x = p.x * sc * 2.0;
        float y = p.y * sc * 2.0;
        float z = st * 10.0 + 10.0;           // z seeded from time, not UV
        for (int i = 0; i < 6; i++) {
            float dx =  sigma * (y - x);
            float dy =  x * (rho - z) - y;
            float dz =  x * y - beta * z;
            x += dx * dt;  y += dy * dt;  z += dz * dt;
        }
        return p + clamp(vec2(x, y) * 0.012, -1.5, 1.5) * sw;
    }

    // ── Mode 6: Clifford — Clifford strange attractor warp ───────────────────
    // xₙ₊₁ = sin(a·yₙ) + c·cos(a·xₙ)
    // yₙ₊₁ = sin(b·xₙ) + d·cos(b·yₙ)
    // a,b,c,d driven by strength and scale.  The attractor basin drives the UV
    // displacement, producing swirling asymmetric folded structures.
    if (u_chaos_mode == 6) {
        float a = -1.4 + sw * 0.6;
        float b =  1.6 - sw * 0.3;
        float c2=  1.0;
        float d =  0.7 + sw * 0.3;
        vec2 h = p * sc;
        for (int i = 0; i < 6; i++) {
            h = vec2(sin(a * h.y) + c2 * cos(a * h.x),
                     sin(b * h.x) + d  * cos(b * h.y));
        }
        return p + clamp(h * 0.15, -1.5, 1.5) * sw * 0.5;
    }

    // ── Mode 7: Ikeda — Ikeda laser-cavity map warp ──────────────────────────
    // Complex map: z_{n+1} = 1 + μ·z_n·exp(i·t_n)
    //   where t_n = 0.4 − 6/(1+|z_n|²)
    // μ (gain) sweeps from 0.6 (ordered) → 0.95 (chaotic) with strength.
    // Produces spiralling laser-cavity-style chaotic structures.
    if (u_chaos_mode == 7) {
        float mu = 0.6 + sw * 0.35;
        vec2 h = p * sc;
        for (int i = 0; i < 6; i++) {
            float t  = 0.4 - 6.0 / (1.0 + dot(h, h));
            float cr = cos(t), sr = sin(t);
            h = vec2(1.0 + mu * (h.x * cr - h.y * sr),
                           mu * (h.x * sr + h.y * cr));
        }
        return p + clamp(h * 0.08, -1.5, 1.5) * sw;
    }

    return p;
}

// ════════════════════════════════════════════════════════════════════════════════
// VIDEO FILTER LIBRARY  (GIMP-inspired)
// apply_color_filter: color-only filters, no texture lookups — works on any stream.
// sample_overlay_filtered: spatial filters for the overlay (needs screen UV).
// blend_streams: GIMP layer blend modes between two RGB colours.
// ════════════════════════════════════════════════════════════════════════════════

// ── Noise helper for film grain ───────────────────────────────────────────────
float _grain(vec2 uv, float t) {
    return fract(sin(dot(uv * 1000.0 + t * 37.3, vec2(12.9898, 78.233))) * 43758.545);
}

// ── Color-only filter (IDs 0–11) ─────────────────────────────────────────────
// a, b = filter parameters (see UI labels for meaning per filter).
vec3 apply_color_filter(vec3 col, int mode, float a, float b) {
    if (mode == 0) return col;  // 0 — None

    // 1 — Brightness / Contrast
    // a = brightness offset (-1..1)   b = contrast multiplier (0..2)
    if (mode == 1) return clamp((col + a) * b, 0.0, 1.0);

    // 2 — Saturation  (a = 0 → greyscale, 1 → normal, 2 → vivid)
    if (mode == 2) {
        float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));
        return clamp(mix(vec3(lum), col, a), 0.0, 1.0);
    }

    // 3 — Hue Rotate  (a = rotation 0..1 wrapping)
    if (mode == 3) {
        vec3 hsl = rgb2hsl(col);
        hsl.x = fract(hsl.x + a);
        return hsl2rgb(hsl);
    }

    // 4 — Posterize  (a = number of levels 2..16)
    if (mode == 4) {
        float lvl = max(2.0, a);
        return floor(col * lvl + 0.5) / lvl;
    }

    // 5 — Invert
    if (mode == 5) return 1.0 - col;

    // 6 — Sepia
    if (mode == 6) {
        float g = dot(col, vec3(0.299, 0.587, 0.114));
        vec3 sepia = vec3(g * 1.08, g * 0.88, g * 0.62);
        return clamp(mix(col, sepia, a), 0.0, 1.0);
    }

    // 7 — Threshold  (a = split point 0..1)
    if (mode == 7) {
        float lum = dot(col, vec3(0.299, 0.587, 0.114));
        return vec3(step(a, lum));
    }

    // 8 — Solarize  (partial invert above threshold a)
    if (mode == 8) {
        return mix(col, 1.0 - col, step(a, col));
    }

    // 9 — Warm  (push reds/yellows, a = strength)
    if (mode == 9) return clamp(col + vec3(a*0.2, a*0.07, -a*0.1), 0.0, 1.0);

    // 10 — Cool  (push blues/cyans, a = strength)
    if (mode == 10) return clamp(col + vec3(-a*0.1, a*0.04, a*0.2), 0.0, 1.0);

    // 11 — Vibrance  (boost unsaturated colours, leave saturated ones alone)
    if (mode == 11) {
        float maxC = max(col.r, max(col.g, col.b));
        float minC = min(col.r, min(col.g, col.b));
        float sat = maxC - minC;
        float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));
        float boost = a * (1.0 - sat);     // more boost where already desaturated
        return clamp(mix(vec3(lum), col, 1.0 + boost), 0.0, 1.0);
    }

    return col;
}

// ── Spatial filters for overlay (IDs 12–18, require screen UV) ───────────────
// Samples the overlay texture with spatial operations.
// texelSz = vec2(1)/u_overlay_size.
vec3 sample_overlay_filtered(vec2 uv, int mode, float a, float b, vec2 texelSz) {
    // For non-spatial modes, just sample and let apply_color_filter handle it
    if (mode < 12) return texture(u_overlay_tex, uv).rgb;

    // 12 — Pixelate / Mosaic  (a = block size in screen fraction)
    if (mode == 12) {
        float sz = max(texelSz.x, a * 0.05);
        vec2 blocked = floor(uv / sz) * sz + sz * 0.5;
        return texture(u_overlay_tex, blocked).rgb;
    }

    // 13 — Ripple / Wave  (a = amplitude, b = frequency)
    if (mode == 13) {
        vec2 rUV = uv + vec2(
            sin(uv.y * b * 20.0 + u_time * 3.0) * a * 0.04,
            cos(uv.x * b * 20.0 + u_time * 2.5) * a * 0.04
        );
        return texture(u_overlay_tex, clamp(rUV, 0.0, 1.0)).rgb;
    }

    // 14 — Edge Detect (Sobel)
    if (mode == 14) {
        vec2 t = texelSz;
        vec3 tl = texture(u_overlay_tex, uv + vec2(-t.x,  t.y)).rgb;
        vec3 tc = texture(u_overlay_tex, uv + vec2( 0.0,  t.y)).rgb;
        vec3 tr = texture(u_overlay_tex, uv + vec2( t.x,  t.y)).rgb;
        vec3 ml = texture(u_overlay_tex, uv + vec2(-t.x,  0.0)).rgb;
        vec3 mr = texture(u_overlay_tex, uv + vec2( t.x,  0.0)).rgb;
        vec3 bl = texture(u_overlay_tex, uv + vec2(-t.x, -t.y)).rgb;
        vec3 bc = texture(u_overlay_tex, uv + vec2( 0.0, -t.y)).rgb;
        vec3 br = texture(u_overlay_tex, uv + vec2( t.x, -t.y)).rgb;
        vec3 Gx = -tl + tr - 2.0*ml + 2.0*mr - bl + br;
        vec3 Gy = -tl - 2.0*tc - tr + bl + 2.0*bc + br;
        return clamp(sqrt(Gx*Gx + Gy*Gy) * a, 0.0, 1.0);
    }

    // 15 — Emboss
    if (mode == 15) {
        vec2 t = texelSz;
        vec3 c0 = texture(u_overlay_tex, uv).rgb;
        vec3 cx = texture(u_overlay_tex, uv + vec2(t.x, t.y)).rgb;
        return clamp((c0 - cx) * a + 0.5, 0.0, 1.0);
    }

    // 16 — Sharpen  (a = strength 0..3)
    if (mode == 16) {
        vec2 t = texelSz;
        vec3 c = texture(u_overlay_tex, uv).rgb;
        vec3 blur = (texture(u_overlay_tex, uv + vec2( t.x, 0.0)).rgb +
                     texture(u_overlay_tex, uv + vec2(-t.x, 0.0)).rgb +
                     texture(u_overlay_tex, uv + vec2(0.0,  t.y)).rgb +
                     texture(u_overlay_tex, uv + vec2(0.0, -t.y)).rgb) * 0.25;
        return clamp(c + (c - blur) * a, 0.0, 1.0);
    }

    // 17 — Bloom / Glow  (a = threshold, b = radius steps 1..4)
    if (mode == 17) {
        vec3 base = texture(u_overlay_tex, uv).rgb;
        vec3 bloom = vec3(0.0);
        float weight = 0.0;
        int steps = int(clamp(b, 1.0, 4.0));
        for (int dx = -steps; dx <= steps; dx++) {
            for (int dy = -steps; dy <= steps; dy++) {
                vec2 off = vec2(float(dx), float(dy)) * texelSz * 3.0;
                vec3 s = texture(u_overlay_tex, uv + off).rgb;
                float bright = dot(s, vec3(0.299, 0.587, 0.114));
                float w = max(0.0, bright - a);
                bloom += s * w;
                weight += w;
            }
        }
        if (weight > 0.0) bloom /= weight;
        return clamp(base + bloom * 0.6, 0.0, 1.0);
    }

    // 18 — Film Grain  (a = strength)
    if (mode == 18) {
        vec3 c = texture(u_overlay_tex, uv).rgb;
        float g = _grain(uv, u_time) * 2.0 - 1.0;
        return clamp(c + g * a * 0.15, 0.0, 1.0);
    }

    return texture(u_overlay_tex, uv).rgb;
}

// ── GIMP layer blend modes ────────────────────────────────────────────────────
// a = base (fractal+primary),  b = overlay layer,  t = overlay_blend (opacity)
vec3 blend_streams(vec3 a, vec3 b, float t, int mode) {
    vec3 blended;
    if (mode == 0)  blended = b;                                          // Normal
    if (mode == 1)  blended = a * b;                                      // Multiply
    if (mode == 2)  blended = 1.0 - (1.0-a)*(1.0-b);                    // Screen
    if (mode == 3)  blended = mix(2.0*a*b, 1.0-2.0*(1.0-a)*(1.0-b),    // Overlay
                                  step(0.5, a));
    if (mode == 4) {                                                       // Soft Light
        vec3 d = mix(sqrt(a), 2.0*a-1.0, step(0.5, b));
        blended = a + (2.0*b-1.0)*d;
    }
    if (mode == 5)  blended = mix(2.0*a*b, 1.0-2.0*(1.0-a)*(1.0-b),    // Hard Light
                                  step(0.5, b));
    if (mode == 6)  blended = abs(a - b);                                 // Difference
    if (mode == 7)  blended = a + b - 2.0*a*b;                           // Exclusion
    if (mode == 8)  blended = clamp(a / max(1.0-b, 0.001), 0.0, 1.0);   // Color Dodge
    if (mode == 9)  blended = 1.0-clamp((1.0-a)/max(b,0.001),0.0,1.0);  // Color Burn
    if (mode == 10) blended = min(a, b);                                  // Darken
    if (mode == 11) blended = max(a, b);                                  // Lighten
    if (mode == 12) blended = clamp(a + b, 0.0, 1.0);                    // Addition
    return mix(a, clamp(blended, 0.0, 1.0), t);
}

// ════════════════════════════════════════════════════════════════════════════════
// MAIN
// ════════════════════════════════════════════════════════════════════════════════
void main() {
    vec2 aspect = vec2(u_resolution.x/u_resolution.y, 1.0);
    vec2 p = (v_uv-0.5)*aspect*2.0/u_zoom + u_offset;

    // Mirror / kaleidoscope folds (applied first, in world space)
    p = apply_mirror(p);
    p = apply_kaleid(p);

    // Domain warp: pre-distort the plane with the SDF gradient before iteration
    if (u_blend_euclidean > 0.001 && u_geo_warp > 0.001) {
        float d = sdf_eval(p);
        p -= normalize(sdf_grad(p)) * sign(d) * u_geo_warp * u_geo_radius * 0.25;
    }

    // Chaos domain warp: applied after SDF warp, before fractal iteration
    p = apply_chaos(p, u_time);

    // Pixel coordinate injection: adds world-space position to iteration seed,
    // making screen location a live variable inside the equation.
    vec2 pix_inject = p * u_pixel_weight * 0.12;

    // Multi-layer repetition: accumulate spatially-offset iterations and average.
    // Layer offsets are arranged at 90-degree intervals around the centre.
    int  nlayers = clamp(u_layer_count, 1, 4);
    float em = 0.0, ej = 0.0, emb = 0.0;

    for (int l = 0; l < 4; l++) {
        if (l >= nlayers) break;
        float ang = float(l) * 1.5708;          // 0 / 90 / 180 / 270 degrees
        vec2  ls  = (l == 0) ? vec2(0.0)
                              : u_layer_offset * vec2(cos(ang), sin(ang));
        vec2  lp   = p + ls;
        vec2  linj = pix_inject + ls * 0.15;

        if (u_blend_mandelbrot > 0.001) em  += iterate(vec2(0.0), lp  + linj);
        if (u_blend_julia      > 0.001) ej  += iterate(lp,        u_julia_c + linj);
        if (u_blend_mandelbulb > 0.001) emb += mandelbulb_slice(lp);
    }
    em  /= float(nlayers);
    ej  /= float(nlayers);
    emb /= float(nlayers);

    // Pure SDF pass when all fractal weights are zero
    float eSDF = 0.0;
    if (u_blend_mandelbrot<0.001 && u_blend_julia<0.001 && u_blend_mandelbulb<0.001)
        eSDF = clamp(1.0-abs(sdf_eval(p))*8.0, 0.0, 1.0)*u_blend_euclidean;

    // Differential flow field (5th blend)
    float ed = (u_blend_diff > 0.001) ? diff_flow(p) : 0.0;

    float total  = u_blend_mandelbrot + u_blend_julia + u_blend_mandelbulb
                 + u_blend_diff + 1e-6;
    float escape = (u_blend_mandelbrot*em + u_blend_julia*ej
                  + u_blend_mandelbulb*emb + u_blend_diff*ed) / total
                 + eSDF;
    escape = clamp(escape, 0.0, 1.0);

    vec2 vidUV = vec2(fract(escape*3.7+u_time*0.05), fract(escape*5.3+0.5));
    vec3 video  = apply_color_filter(texture(u_video_tex, vidUV).rgb,
                                     u_vid_filter, u_vid_fa, u_vid_fb);

    // ── Base palette ──────────────────────────────────────────────────────────
    vec3 baseColor = palette(escape + u_time*0.08);

    // ── Color Synthesizer ─────────────────────────────────────────────────────
    if (u_cs_enabled) {
        // Build primary and alternate colours for this escape value
        vec3 col1 = synthPalette(escape, u_cs_hsl);
        vec3 col2 = synthPalette(escape, u_cs_hsl_alt);
        vec3 synthCol = mix(col1, col2, u_cs_alt_blend);

        if (u_cs_mode == 0) {
            // Replace: synth drives all colour; palette provides detail variation
            baseColor = synthCol;
        } else if (u_cs_mode == 1) {
            // Multiply: tints the palette with the synth colour
            baseColor = baseColor * synthCol * 2.0;
        } else {
            // Screen: lightens — good for dark fractals
            baseColor = 1.0 - (1.0-baseColor)*(1.0-synthCol);
        }
    }

    vec3 color  = mix(baseColor, video, 0.65+0.35*escape);
    color *= step(0.001, escape)*0.95 + 0.05;

    // ── Overlay video layer — filter + GIMP blend mode ───────────────────────
    if (u_overlay_blend > 0.0) {
        vec2 texelSz = (u_overlay_size.x > 1.0) ? 1.0 / u_overlay_size : vec2(0.001);
        vec3 ovr = sample_overlay_filtered(v_uv, u_ovr_filter, u_ovr_fa, u_ovr_fb, texelSz);
        // Apply color-only overlay filter on top of spatial result (mode < 12)
        if (u_ovr_filter < 12)
            ovr = apply_color_filter(ovr, u_ovr_filter, u_ovr_fa, u_ovr_fb);
        color = blend_streams(color, ovr, u_overlay_blend, u_stream_blend_mode);
    }

    fragColor = vec4(color, 1.0);
}
