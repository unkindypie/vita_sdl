#version 120
attribute vec3 position;
attribute vec4 color;
attribute vec4 surface;
uniform mat4 mvp;
uniform vec4 plane;
uniform vec4 light;
varying vec4 tint;
varying vec4 info;
varying float height;
void main() {
    gl_Position = mvp * vec4(position, 1.0);
    tint = color;
    info = surface;
    height = surface.w > 1.5 ? 1.0 : dot(vec4(position, 1.0), plane);
    if (surface.w > 0.5 && surface.w < 1.5)
        info.z = clamp(dot(normalize(color.xyz * 2.0 - 1.0), light.xyz / max(length(light.xyz),0.0001)) * (1.0-light.w) + light.w, 0.0, 1.0);
}
