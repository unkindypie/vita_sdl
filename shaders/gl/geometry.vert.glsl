#version 120
attribute vec3 position;
attribute vec4 color;
attribute vec4 surface;
uniform mat4 mvp;
uniform mat4 normals;
uniform vec4 plane;
uniform vec4 light;
varying vec4 tint;
varying vec4 info;
varying float height;
void main() {
    gl_Position = mvp * vec4(position, 1.0);
    tint = color;
    info = surface;
    height = surface.w > 1.5 && surface.w < 2.5 ? 1.0 : dot(vec4(position, 1.0), plane);
    if (surface.w > 0.5 && surface.w < 1.5)
        info.z = clamp(dot(normalize(color.xyz * 2.0 - 1.0), light.xyz / max(length(light.xyz),0.0001)) * (1.0-light.w) + light.w, 0.0, 1.0);
    if (surface.w > 2.5) {
        vec3 normal = normalize((normals * vec4(surface.xyz, 0.0)).xyz);
        float diffuse = dot(normal, light.xyz / max(length(light.xyz), 0.0001));
        // Authored Asciicker mesh shading uses a wrapped diffuse term. Keep
        // its lift rather than introducing harsh Lambert-only silhouettes.
        tint.rgb *= clamp(diffuse*(1.0-0.5*light.w)+0.5*light.w+0.5, 0.0, 1.0);
    }
}
