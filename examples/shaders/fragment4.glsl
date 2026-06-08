#version 120 
 uniform float iTime; 
 uniform vec2 iResolution;
 uniform vec4 iMouse;
 ///////////////-------------------------------------------------

#define R iResolution
#define t iTime / 1.4
#define lngth( x , y ) pow( 4.37 - length( x ) - length( y ) / 4. , .4 )

vec4 bgr( vec2 I )
{
  I = I / R.y;
  I.x += sin( t * 2. ) * cos( t ) / 2.;
  return vec4( mod( ceil( I * 20. ).x - ceil( I * 20. ).y, 2. ) );
}

vec2 bubble( vec2 u, float s )
{
    u += vec2( sin( t * 2. ) * cos( t /2.5 ) , cos( 2. * sin( t ) ) ) * s;
    u *=  dot( -u * 90. , u );
    return  length( u / 3. ) < 1.4 ? u : vec2( 0 );
}

void mainImage( out vec4 O, vec2 I )
{
    vec2 u = (2.* I - R.xy ) / R.y ;
    vec2 b1 = bubble( u / 1.2 , 1. );
    vec2 b2 = bubble( u / 1.2 , -1.);
    vec2 b3 = bubble( -b1 / 7., -1.) * 1.8 ;
    vec2 b4 = bubble( -b2 / 7.,  1.) * 1.8;

    vec2 bubbles =  2. * I - 19. * ( b1 + b2 + b3 + b4 );
    O = bgr( bubbles ) * min( lngth( b1 , b3 ), lngth( b2 , b4 ) );
    O *= O / 1.4;
}
  
///////////////-------------------------------------------------  
  
void main() {
	 mainImage(gl_FragColor,  gl_FragCoord.xy);
}
	