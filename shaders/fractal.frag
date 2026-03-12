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
uniform int   u_formula;           // formula A
uniform int   u_formula_b;         // formula B  (blend crossfades A→B)
uniform float u_formula_blend;     // 0=pure A  1=pure B
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
// FORMULA EVALUATOR
// Applies one iteration step for the given formula ID.
// Newton (f==8) returns the Newton-step result; convergence is checked in iterate().
// ════════════════════════════════════════════════════════════════════════════════
vec2 eval_formula(int f, vec2 z, vec2 z_prev, vec2 seed) {
    if (f == 0) return csqr(z) + seed;
    if (f == 1) return csin(z) + seed;
    if (f == 2) return cexp(z) + seed;
    if (f == 3) return ccos(z) + seed;
    if (f == 4) return csinh(z) + seed;
    if (f == 5) return ccosh(z) + seed;
    if (f == 6) return csqr(vec2(abs(z.x), abs(z.y))) + seed;
    if (f == 7) return csqr(cconj(z)) + seed;
    if (f == 8) {
        vec2 den = 3.0 * csqr(z);
        if (cabs2(den) < 1e-12) return z;
        return z - cdiv(ccube(z) - vec2(1.0, 0.0), den);
    }
    if (f == 9) return csqr(z) + vec2(seed.x, 0.0) + seed.y * z_prev;
    if (f == 10) return cpow_r(z, u_power) + seed;
    return csqr(z) + seed;
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
    vec3 color  = mix(palette(escape+u_time*0.08), video, 0.65+0.35*escape);
    color *= step(0.001, escape)*0.95 + 0.05;

    fragColor = vec4(color, 1.0);
}
