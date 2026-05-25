#version 120 
 uniform float iTime; 
 uniform vec2 iResolution;
 uniform vec4 iMouse;
 ///////////////-------------------------------------------------

// --- Compiled Custom Parameters for Shadertoy ---
#define uSpeed 3.1000
#define uFrequency 22.0000
#define uChladniOrder 6.0000
#define uDamping 0.3000
#define uGlossiness 55.0000
#define uNoise 0.3500
// -------------------------------------------------

// Cymatic wave height field calculation
float cymaticHeight(vec2 p, float t) {
    float r = length(p);
    float theta = atan(p.y, p.x);
    
    // Fluid turbulence / micro-vibration
    float turbulence = sin(p.x * 12.0 + t) * cos(p.y * 12.0 - t) * uNoise * 0.12;
    r += turbulence;

    // Mode A: Pure concentric (Radial) standing waves
    float modeA = cos(r * uFrequency - t * 2.5) / (1.0 + r * r * uDamping);
    
    // Mode B: Chladni geometric standing wave pattern (angular resonance)
    float modeB = cos(r * uFrequency * 1.3 - t * 1.5) * cos(theta * uChladniOrder) / (1.0 + r * uDamping);
    
    // Mode C: Square-grid boundary interference resonance
    float modeC = (cos(p.x * uFrequency * 0.8 + t) * sin(p.y * uFrequency * 0.8 + t)) * 0.4 / (1.0 + r * 0.5);
    
    return modeA * 0.45 + modeB * 0.35 + modeC * 0.2;
}

// Compute normal using finite differences
vec3 getNormal(vec2 p, float t) {
    float eps = 0.008;
    float h = cymaticHeight(p, t);
    float h_x = cymaticHeight(p + vec2(eps, 0.0), t);
    float h_y = cymaticHeight(p + vec2(0.0, eps), t);
    
    float strength = 0.14; 
    vec3 N = normalize(vec3((h - h_x) / eps * strength, (h - h_y) / eps * strength, 1.0));
    return N;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
    vec2 uv = fragCoord.xy / iResolution.xy;
    // Center UV coordinate system with aspect correction
    vec2 p = (fragCoord.xy - 0.5 * iResolution.xy) / iResolution.y * 2.0;

    float t = iTime * uSpeed;

    // Height & Normal derivation
    vec3 N = getNormal(p, t);
    float h = cymaticHeight(p, t);
    
    // Fake chromatic dispersion by shifting sampling coordinates slightly along the normal vector
    float hr = cymaticHeight(p + N.xy * 0.015, t);
    float hg = cymaticHeight(p + N.xy * 0.008, t);
    float hb = cymaticHeight(p, t);
    
    // Deep water color base gradient
    vec3 baseColor = vec3(0.01, 0.03, 0.09);
    vec3 waterScatter = vec3(0.0, 0.45, 0.65) * (h * 0.5 + 0.5);
    
    // Dynamic chromatic refract highlights
    vec3 chromatics = vec3(
        smoothstep(-0.3, 0.7, hr),
        smoothstep(-0.3, 0.7, hg),
        smoothstep(-0.3, 0.7, hb)
    ) * vec3(0.2, 0.8, 1.0);

    // Resonance nodes intensity (glowing physical nodes of the wave)
    vec3 resonanceGlow = vec3(0.4, 0.1, 0.9) * pow(abs(h), 2.5) * 1.8;

    // Camera view vector
    vec3 V = vec3(0.0, 0.0, 1.0); 
    
    // Two studio lights (Cool cyan and warm magenta) for dramatic liquid look
    vec3 lPos1 = vec3(1.5, 1.5, 2.0);
    vec3 lPos2 = vec3(-1.5, -1.5, 2.0);
    
    vec3 lDir1 = normalize(lPos1);
    vec3 lDir2 = normalize(lPos2);
    
    // Specular reflections
    vec3 r1 = reflect(-lDir1, N);
    vec3 r2 = reflect(-lDir2, N);
    
    float spec1 = pow(max(dot(r1, V), 0.0), uGlossiness);
    float spec2 = pow(max(dot(r2, V), 0.0), uGlossiness);
    
    vec3 specColor1 = vec3(0.0, 0.9, 1.0) * spec1 * 2.0;
    vec3 specColor2 = vec3(1.0, 0.15, 0.65) * spec2 * 1.5;
    
    // Fresnel reflection
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 5.0);
    vec3 skyReflection = vec3(0.5, 0.8, 1.0) * fresnel * 0.5;
    
    // Combine color layers
    vec3 finalColor = baseColor + waterScatter + chromatics * 0.45 + resonanceGlow + specColor1 + specColor2 + skyReflection;
    
    // Focal depth vignette
    float d = length(p);
    float vignette = smoothstep(1.8, 0.3, d);
    finalColor *= vignette;
    
    // Gamma correction and dynamic range boost
    finalColor = pow(finalColor, vec3(0.95));
    
    fragColor = vec4(finalColor, 1.0);
}

  
///////////////-------------------------------------------------  
  
void main() {
	 mainImage(gl_FragColor,  gl_FragCoord.xy);
}
	