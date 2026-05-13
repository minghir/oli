#version 120 
uniform float iTime; 
uniform vec2 iResolution;
uniform vec4 iMouse;

#define R iResolution
#define PI 3.14159265
#define HD 1.41421356 // sqrt(2.)
#define HT 1.73205081 // sqrt(3.)
#define ROT(a) mat2(cos(a), -sin(a), sin(a), cos(a)) 

// Protecție: dacă nu miști mouse-ul, M va fi 0.001 (ca să nu înghețe timpul)
#define M (dot(iMouse, iMouse) / 1000000.0 + 0.001)
#define T (iTime / 1.4 * M / 0.5)

float poly(vec2 uv, float sizes, float size_len){
    vec2 st = vec2(atan(uv.x, uv.y), length(uv));
    float angle = st.x;
    float r = cos(PI/sizes) / cos(angle - 2.0 * PI / sizes * floor((sizes * angle + PI) / (2.0 * PI)));
    float d = r * size_len - st.y;
    float ds = smoothstep(0.0, 1.0 / R.y, d);
    float ss = smoothstep(0.0, 3.0 / R.y, d);
    return 2.0 * ds - ss * ss;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = (fragCoord.xy - 0.5 * R.xy) / R.y;
    float size = 1.0 / 7.0;

    float r_o = size / sqrt(2.0 - HD);
    float a_o = r_o * sqrt(0.5 + 0.25 * HD);
    
    float r_h = size;
    float a_h = r_h * 0.5 * HT;

    float r_p = size / HD;
    float a_p = r_p * HD / 2.0;
    
    float r_t = size / HT;
    float a_t = r_t * 0.5;
    
    float srot = (360.0 / 16.0) * PI / 180.0;
    float rp_o_h = 8.0 / 6.0;
    float rp_h_p = 6.0 / 4.0;
    float rp_p_t = 4.0 / 3.0;
    
    uv.y += 0.5 - (r_o - a_o);
    
    // Octogon
    vec2 ouv = uv;
    ouv.y -= a_o;
    ouv *= ROT(srot);
    ouv *= ROT(-T);
    float o = poly(ouv, 8.0, r_o);
    
    // Hexagon
    vec2 huv = uv;
    float d_h = (r_o - a_o) + (r_h - a_h);
    huv.y -= (2.0 * a_o + a_h) + d_h;
    huv.y -= d_h * abs(cos(T * 4.0)) - d_h;
    huv *= ROT(srot * rp_o_h);
    huv *= ROT(T * rp_o_h);
    float h = poly(huv, 6.0, r_h);
    
    // Patrat
    vec2 puv = uv;
    float d_p = (r_o - a_o) + 2.0 * (r_h - a_h) + (r_p - a_p);
    puv.y -= (2.0 * a_o + 2.0 * a_h + a_p) + d_p;
    puv.y -= d_p * abs(cos(T * 4.0)) - d_p;
    puv *= ROT(srot * rp_o_h * rp_h_p);
    puv *= ROT(-T * rp_o_h * rp_h_p);
    float p = poly(puv, 4.0, r_p);
    
    // Triunghi 
    vec2 tuv = uv;
    float d_t = (r_o - a_o) + 2.0 * (r_h - a_h) + 2.0 * (r_p - a_p) + (r_t - a_t);
    tuv.y -= (2.0 * a_o + 2.0 * a_h + 2.0 * a_p + a_t) + d_t;
    tuv.y -= d_t * abs(cos(T * 4.0)) - d_t;
    tuv *= ROT(T * rp_o_h * rp_h_p * rp_p_t);
    float t = poly(tuv, 3.0, r_t);

    vec3 col = vec3(252.0/255.0, 244.0/255.0, 228.0/255.0);
    col = mix(col, vec3(38.0/255.0, 111.0/255.0, 108.0/255.0), o);
    col = mix(col, vec3(255.0/255.0, 175.0/255.0, 0.0), h);
    col = mix(col, vec3(143.0/255.0, 14.0/255.0, 1.0/255.0), p);
    col = mix(col, vec3(75.0/255.0, 0.0, 130.0/255.0), t);
    
    fragColor = vec4(col, 1.0);
}

void main() {
    mainImage(gl_FragColor, gl_FragCoord.xy);
}