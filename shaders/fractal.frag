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

    return p;
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
    vec3 video  = texture(u_video_tex, vidUV).rgb;

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

    // ── Overlay video layer (50 % blend by default) ───────────────────────────
    if (u_overlay_blend > 0.0) {
        vec3 overlay = texture(u_overlay_tex, v_uv).rgb;
        color = mix(color, overlay, u_overlay_blend);
    }

    fragColor = vec4(color, 1.0);
}
