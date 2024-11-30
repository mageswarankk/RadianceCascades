// Not in use right now

#version 430 core

#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

in vec2 uv;
in vec4 color;

out vec4 FragColor;

uniform sampler2D u_finalRender;

void main() {
    vec2 fixedUv = (uv + 1.0f) / 2.0f;
    FragColor = texture(u_finalRender, fixedUv);
}
