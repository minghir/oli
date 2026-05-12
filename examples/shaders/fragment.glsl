#version 120 
uniform float time; 
void main() { 
	vec2 res = vec2(900.0, 700.0); 
	vec2 I = gl_FragCoord.xy; 
	I = I * 2.0 - res; 
	float d = dot(I, I); 
	float a = atan(I.x, I.y); 
	vec4 O = vec4(sin(log(d) + a - time * 3.0) + 0.9); 
	O = 0.5 + O / fwidth(O); 
	gl_FragColor = vec4(O.rgb, 1.0);
}