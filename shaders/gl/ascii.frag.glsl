#version 120
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define frac fract
#define tex2D texture2D
#define lerp mix
uniform sampler2D scene;
uniform sampler2D materials;
uniform sampler2D font;
uniform sampler2D overlay;
uniform sampler2D underlay;
uniform sampler2D reflection;
uniform sampler2D styles;
uniform float clock;
#include "../common/ascii.cg"
varying vec2 uv;
uniform vec4 layout;
void main() {
    gl_FragColor = resolve_ascii(uv, layout);
}
