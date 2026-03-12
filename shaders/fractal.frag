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
uniform vec2  u_julia_c;
uniform float u_power;
uniform int   u_max_iter;
uniform float u_bailout;
uniform float u_zoom;
uniform vec2  u_offset;
uniform int   u_formula;        // iteration formula selector (see table)
uniform float u_formula_blend;  // 0=classic z²+c  1=chosen formula
uniform float u_geo_warp;       // SDF orbit-trap warp strength
uniform int   u_geo_shape;
uniform int   u_geo_sides;
uniform float u_geo_radius;
uniform float u_geo_rotation;
uniform bool  u_geo_tile;
uniform sampler2D u_video_tex;

// ════════════════════════════════════════════════════════════════════════════════
// COMPLEX NUMBER LIBRARY  (Fractal Explorer 2 / UltraFractal function set)
// z = vec2(real, imaginary)
// ════════════════════════════════════════════════════════════════════════════════
vec2 cmul (vec2 a, vec2 b) { return vec2(a.x*b.x-a.y*b.y, a.x*b.y+a.y*b.x); }
vec2 csqr (vec2 z)         { return vec2(z.x*z.x-z.y*z.y, 2.0*z.x*z.y); }
vec2 ccube(vec2 z)         { return cmul(csqr(z), z); }
vec2 cconj(vec2 z)         { return vec2(z.x, -z.y); }
float cabs2(vec2 z)        { return dot(z,z); }
vec2 cinv  (vec2 z)        { return cconj(z)/dot(z,z); }
vec2 cdiv  (vec2 a, vec2 b){ return cmul(a, cinv(b)); }

// Exponential / logarithm
vec2 cexp(vec2 z) { return exp(z.x)*vec2(cos(z.y), sin(z.y)); }
vec2 clog(vec2 z) { return vec2(log(max(length(z),1e-10)), atan(z.y,z.x)); }

// Real power:  z^n  via polar form
vec2 cpow_r(vec2 z, float n) {
    if (length(z) < 1e-10) return vec2(0.0);
    float th = atan(z.y, z.x)*n;
    return pow(length(z), n)*vec2(cos(th), sin(th));
}
// Complex power: z^w = exp(w·log z)
vec2 cpow_c(vec2 z, vec2 w) { return cexp(cmul(w, clog(z))); }

// Trigonometric (complex domain)
vec2 csin (vec2 z) { return vec2( sin(z.x)*cosh(z.y),  cos(z.x)*sinh(z.y)); }
vec2 ccos (vec2 z) { return vec2( cos(z.x)*cosh(z.y), -sin(z.x)*sinh(z.y)); }
vec2 ctan (vec2 z) { return cdiv(csin(z), ccos(z)); }

// Hyperbolic (complex domain)
vec2 csinh(vec2 z) { return vec2(sinh(z.x)*cos(z.y), cosh(z.x)*sin(z.y)); }
vec2 ccosh(vec2 z) { return vec2(cosh(z.x)*cos(z.y), sinh(z.x)*sin(z.y)); }
vec2 ctanh(vec2 z) { return cdiv(csinh(z), ccosh(z)); }

// Square root
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
// Gradient via central differences (for orbit warp direction)
vec2 sdf_grad(vec2 p) {
    const float h=0.0015;
    float d=sdf_eval(p);
    return vec2(sdf_eval(p+vec2(h,0.0))-d, sdf_eval(p+vec2(0.0,h))-d)/h;
}

// ════════════════════════════════════════════════════════════════════════════════
// MANDELBULB 2-D SLICE  (z=0 cross-section used when mandelbulb blend < 0.5)
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
// COMBINED FRACTAL + SDF ITERATION
//
// Formula table (u_formula):
//  0  z² + c               Mandelbrot / Julia (classic)
//  1  sin(z) + c           Sinus fractal
//  2  exp(z) + c           Exponential spiral fractal
//  3  cos(z) + c           Cosine fractal
//  4  sinh(z) + c          Hyperbolic sine fractal
//  5  cosh(z) + c          Hyperbolic cosine fractal
//  6  |re|+i|im| → (…)²+c  Burning Ship
//  7  conj(z)² + c         Tricorn / Mandelbar
//  8  z − (z³−1)/(3z²)     Newton convergence (z³=1 roots)
//  9  z²+Re(c)+Im(c)·z₋₁   Phoenix (two-step memory recurrence)
// 10  z^n + c              Arbitrary power (u_power)
//
// SDF orbit trap:
//   During each iteration step the SDF is evaluated at the current z.
//   min-distance is tracked (orbit trap coloring).
//   When u_geo_warp > 0 the SDF gradient MOVES z toward the shape boundary —
//   this algebraically couples the Euclidean geometry into the fractal orbit.
// ════════════════════════════════════════════════════════════════════════════════

const vec2 kRoot0 = vec2( 1.000,  0.000);
const vec2 kRoot1 = vec2(-0.500,  0.866);
const vec2 kRoot2 = vec2(-0.500, -0.866);

float iterate(vec2 z_init, vec2 seed) {
    vec2  z      = z_init;
    vec2  z_prev = z;
    float trap   = 1e6;

    for (int i=0; i<u_max_iter; i++) {

        // ── Choose iteration formula ──────────────────────────────────────────
        vec2 z_classic = csqr(z) + seed;
        vec2 z_alt     = z_classic;

        int f = u_formula;
        if      (f== 1) z_alt = csin(z) + seed;
        else if (f== 2) z_alt = cexp(z) + seed;
        else if (f== 3) z_alt = ccos(z) + seed;
        else if (f== 4) z_alt = csinh(z) + seed;
        else if (f== 5) z_alt = ccosh(z) + seed;
        else if (f== 6) z_alt = csqr(vec2(abs(z.x),abs(z.y))) + seed;
        else if (f== 7) z_alt = csqr(cconj(z)) + seed;
        else if (f== 8) {
            vec2 den = 3.0*csqr(z);
            if (cabs2(den)<1e-12) { trap=0.0; break; }
            z_alt = z - cdiv(ccube(z)-vec2(1,0), den);
        }
        else if (f== 9) z_alt = csqr(z) + vec2(seed.x,0.0) + seed.y*z_prev;
        else if (f==10) z_alt = cpow_r(z, u_power) + seed;

        vec2 z_temp = z;
        z = (u_formula_blend>0.999) ? z_alt
          : (u_formula_blend<0.001) ? z_classic
          : mix(z_classic, z_alt, u_formula_blend);
        z_prev = z_temp;

        // ── SDF orbit trap + warp  (geometry ↔ fractal algebra coupling) ──────
        if (u_blend_euclidean > 0.001) {
            float d = sdf_eval(z);
            trap = min(trap, abs(d));
            if (u_geo_warp > 0.001) {
                vec2 g = sdf_grad(z);
                // Attract z toward shape boundary: bends the fractal orbit
                z -= normalize(g) * sign(d) * u_geo_warp * 0.035;
            }
        }

        // ── Escape / convergence ──────────────────────────────────────────────
        if (f == 8) {
            float nearest = min(length(z-kRoot0), min(length(z-kRoot1),length(z-kRoot2)));
            trap = min(trap, nearest);
            if (nearest < 0.001) return float(i)/float(u_max_iter);
        } else {
            float r2 = dot(z,z);
            if (r2 > u_bailout*u_bailout) {
                float escape_t = float(i)/float(u_max_iter)
                              - log2(log2(max(r2,1.0001)))/float(u_max_iter);
                float trap_c = clamp(1.0-trap*(2.0+u_geo_warp*10.0), 0.0, 1.0);
                // Escape time blends with orbit trap: geometry IS the coloring
                return mix(escape_t, trap_c, clamp(u_blend_euclidean*1.8, 0.0, 1.0));
            }
        }
    }
    // Interior: orbit trap illuminates set interior with the SDF shape
    return clamp(1.0-trap*2.0, 0.0, 1.0) * u_blend_euclidean;
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

    // SDF INPUT domain warp: pre-distort the complex plane with the geometry
    // gradient before any iteration begins (doubles the visual coupling).
    if (u_blend_euclidean > 0.001 && u_geo_warp > 0.001) {
        float d = sdf_eval(p);
        p -= normalize(sdf_grad(p)) * sign(d) * u_geo_warp * u_geo_radius * 0.25;
    }

    float em  = (u_blend_mandelbrot > 0.001) ? iterate(vec2(0.0), p)     : 0.0;
    float ej  = (u_blend_julia      > 0.001) ? iterate(p, u_julia_c)     : 0.0;
    float emb = (u_blend_mandelbulb > 0.001) ? mandelbulb_slice(p)       : 0.0;

    // Pure SDF when all fractal weights are zero
    float eSDF = 0.0;
    if (u_blend_mandelbrot<0.001 && u_blend_julia<0.001 && u_blend_mandelbulb<0.001)
        eSDF = clamp(1.0-abs(sdf_eval(p))*8.0, 0.0, 1.0)*u_blend_euclidean;

    float total  = u_blend_mandelbrot + u_blend_julia + u_blend_mandelbulb + 1e-6;
    float escape = (u_blend_mandelbrot*em + u_blend_julia*ej + u_blend_mandelbulb*emb)/total
                 + eSDF;
    escape = clamp(escape, 0.0, 1.0);

    vec2 vidUV = vec2(fract(escape*3.7+u_time*0.05), fract(escape*5.3+0.5));
    vec3 video  = texture(u_video_tex, vidUV).rgb;
    vec3 color  = mix(palette(escape+u_time*0.08), video, 0.65+0.35*escape);
    color *= step(0.001, escape)*0.95 + 0.05;

    fragColor = vec4(color, 1.0);
}
