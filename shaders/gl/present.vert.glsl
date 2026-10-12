#version 120
attribute vec2 position;
varying vec2 uv;
void main() {
    gl_Position = vec4(position, 0, 1);
    uv = vec2(position.x, -position.y) * 0.5 + 0.5;
}
