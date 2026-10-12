#version 120
#define float4 vec4
#define tex2D texture2D
uniform sampler2D terrain;
#include "../common/surface.cg"
varying vec4 tint;
varying vec4 info;
varying float height;
void main() {
    if (height < 0.0) discard;
    gl_FragColor = resolve_surface(tint, info);
}
