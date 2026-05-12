#version 120 
 uniform float iTime; 
 uniform vec2 iResolution;
 

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



