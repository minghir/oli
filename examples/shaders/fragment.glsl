#version 120 
 uniform float iTime; 
 uniform vec2 iResolution;
 uniform vec4 iMouse;
 ///////////////-------------------------------------------------
/*
#define rot(a) mat2(cos(a),-sin(a),sin(a),cos(a))
void main() {
	vec2 i = gl_FragCoord.xy; 
    float j, t = iTime;
    vec2 r = iResolution, c = vec2(-.7,.27015);
    for(i = (i+i-r)/r.y/5.*rot(t/50.); j++<3e2 && dot(i,i)<4.;)
      i = (vec2(i.x*i.x-i.y*i.y,2.*i.x*i.y)+c)*rot(t/3e3);
    vec4 o = vec4(1.-j/3e2);
	gl_FragColor = vec4(o.rgb, 1.0);
}
*/

// Rotire pe axa Y
mat3 rotateY(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat3(
        c, 0, s,
        0, 1, 0,
        -s, 0, c
    );
}

// Rotire pe axa X
mat3 rotateX(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat3(
        1, 0, 0,
        0, c, -s,
        0, s, c
    );
}

// SDF pentru Octaedru (https://www.shadertoy.com/view/stKSzc)
float sdOctahedron( vec3 p, float s){

  p = abs(p);
  float m = p.x+p.y+p.z-s;
  vec3 q;
       if( 3.0*p.x < m ) q = p.xyz;
  else if( 3.0*p.y < m ) q = p.yzx;
  else if( 3.0*p.z < m ) q = p.zxy;
  else return m*0.57735027;
    
  float k = clamp(0.5*(q.z-q.y+s),0.0,s); 
  return length(vec3(q.x,q.y-s+k,q.z-k)); 
}


// SDF pentru cub
float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

// SDF pentru plan orizontal
float sdPlane(vec3 p) {
    return p.y-.1;

}

// SDF pentru sferă
float sdSphere(vec3 p, float r) {
    return length(p) - r*1.2;
}

// Funcția care definește scena
float map(vec3 p) {
    // Aplicăm rotații și translație cubului
 
    vec3 rotated_p = p;
    rotated_p *= rotateY(iTime*1.2);
    rotated_p *= rotateX(iTime*2.5);
    rotated_p.x += atan(iTime)*2.;
    rotated_p *= rotateY(iTime*5.);

    // Sfere pe cele 6 fețe ale cubului
    float face_spheres = min(
        min(sdSphere(rotated_p - vec3(0.0, 0.0, 1.0), 0.5), sdSphere(rotated_p - vec3(0.0, 0.0, -1.0), 0.5)),
        min(
            min(sdSphere(rotated_p - vec3(-1.0, 0.0, 0.0), 0.5), sdSphere(rotated_p - vec3(1.0, 0.0, 0.0), 0.5)),
            min(sdSphere(rotated_p - vec3(0.0, 1.0, 0.0), 0.5), sdSphere(rotated_p - vec3(0.0, -1.0, 0.0), 0.5))
        )
    );
    // Octaedru 
    float octa_dist = sdOctahedron(rotated_p, 1.5);

    // Cubul propriu-zis
    float cube_dist = sdBox(rotated_p, vec3(.7));

    // Planul orizontal
    float plane_dist = sdPlane(p + vec3(0, 1.5, .0));

    // Combinăm toate obiectele
    return min(octa_dist,min(face_spheres, min(cube_dist, plane_dist)));
}

// Calculul normalei prin diferențe finite
vec3 get_normal(vec3 p) {
    const vec2 h = vec2(0.001, 0.0);
    return normalize(vec3(
        map(p + h.xyy) - map(p - h.xyy),
        map(p + h.yxy) - map(p - h.yxy),
        map(p + h.yyx) - map(p - h.yyx)
    ));
}

// Umbre moi prin raymarching secundar
float softShadow(vec3 ro, vec3 rd) {
    float res = 1.0;
    float t = 0.02;
    for (int i = 0; i < 50; ++i) {
        float h = map(ro + rd * t);
        if (h < 0.001) return 0.0;
        res = min(res, 4.0 * h / t);
        t += h;
        if (t > 5.0) break;
    }
    return clamp(res, 0.0, 1.);
}

// Ambient occlusion simplificat
float ambientOcclusion(vec3 p, vec3 n) {
    float ao = 0.0;
    float sca = 1.0;
    for (int i = 1; i <= 5; ++i) {
        float h = 0.01 * float(i);
        float d = map(p + n * h);
        ao += (h - d) * sca;
        sca *= 0.7;
    }
    
    return clamp(1.0 - ao, 0.0, 1.0);
}

// Funcția principală
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    // Coordonate normalizate
    vec2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
    uv.y-=0.3;
    // Poziția camerei
    float radius = 6.0;
    float angle =  dot(iMouse,iMouse)/1e5;

    vec3 ro = vec3(
        sin(angle) * radius,
        1.5,
        cos(angle) * radius
    );

    // Direcția razei
   vec3 target = vec3(0.0, 1.5, 0.0); // centrul scenei
   vec3 forward = normalize(target - ro);
   vec3 right = normalize(cross(vec3(0, 1, 0), forward));
   vec3 up = cross(forward, right);

    vec3 rd = normalize(forward + uv.x * right + uv.y * up);

    // Raymarching principal
    float t = 0.0;
    vec3 p;
    
    for (int i = 0; i < 500; ++i) {
        
        p = ro + rd * t;
        float d = map(p);
        if (d < 0.001 || t > 20.0) break;
        t += d;
    }
 
    vec3 col = vec3(0.0);
    if (t < 10.0) {
        // Iluminare
        vec3 normal = get_normal(p);
        vec3 light_dir = normalize(vec3(2., 2., -1.)); // lumină animată
                
        vec3 view_dir = normalize(ro - p);
        vec3 half_dir = normalize(light_dir + view_dir);

        float shadow = softShadow(p + normal * 0.01, light_dir);
        float ao = ambientOcclusion(p, normal);

        float shadowSoft = mix(0.09, 1.0, shadow); // umbrele nu coboară sub 0.09
        float diff = max(dot(normal, light_dir), 0.0) * shadowSoft * ao;


        float spec = pow(max(dot(normal, half_dir), 0.0), 32.0);

        // Verificăm dacă am lovit planul
        bool isPlane = abs(map(p) - sdPlane(p + vec3(0, 1.5, 0))) < 0.001;

        if (isPlane) {
            // Reflexie cu blur
            vec3 reflect_dir = reflect(rd, normal);
            vec3 jitter = vec3(
                sin(iTime * 3.0 + p.x * 5.0) * 0.01,
                cos(iTime * 2.0 + p.z * 4.0) * 0.01,
                0.0
            );
            vec3 reflect_dir_blurred = normalize(reflect_dir + jitter);

            // Raymarching pentru reflexie
            float t_reflect = 0.0;
            vec3 p_reflect;
            vec3 p_reflect_start = p + reflect_dir_blurred * 0.01;
            for (int i = 0; i < 50; ++i) {
                p_reflect = p_reflect_start + reflect_dir_blurred * t_reflect;
                float d_reflect = map(p_reflect);
                if (d_reflect < 0.001 || t_reflect > 10.0) break;
                t_reflect += d_reflect;
            }

            vec3 reflected_color = vec3(0.0);
            if (t_reflect < 10.0) {
                vec3 n_reflect = get_normal(p_reflect);
                float diff_reflect = max(dot(n_reflect, light_dir), 0.0);
                float spec_reflect = pow(max(dot(n_reflect, half_dir), 0.0), 64.0);
                reflected_color = vec3(1.0, 0.7, 0.3) * diff_reflect + vec3(1.0) * spec_reflect;
            }

            // Checker pattern pe plan
            // Proiecție ortogonală pe planul y = -1.4
            vec3 planOrigin = vec3(0.0, -1.4, 0.0);
            vec3 planNormal = vec3(0.0, 1.0, 0.0);
            vec3 projected = p - dot(p - planOrigin, planNormal) * planNormal;

            // Checker pattern stabil
            float scale = 1.0;
            float pattern = mod(floor(projected.x * scale) + floor(projected.z * scale), 2.0);
            float checker = sin(projected.x * 3.1415) * sin(projected.z * 3.1415);
            float base = mix(0.3, 1.1, smoothstep(0.0, 0.01, checker));


            vec3 base_color = vec3(base) * diff + vec3(0.05) + vec3(1.0) * spec;



            // Efect Fresnel pentru reflexie
            float fresnel = pow(1.0 - dot(normal, view_dir), 2.0);
            col = mix(base_color, reflected_color, fresnel * 1.5);
        } else {
            // Culoare pentru obiecte
            col = vec3(0.1) + vec3(1.0, 0.5, 0.0) * diff + vec3(1.0) * spec;
        }

        fragColor = vec4(col, 1.0);
    } else {
        // Fundal negru
        fragColor = vec4(.0);
      }
  }
  
///////////////-------------------------------------------------  
  
void main() {
	 mainImage(gl_FragColor,  gl_FragCoord.xy);
}
	