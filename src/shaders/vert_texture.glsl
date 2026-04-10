#version 400

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_uv;

uniform mat4 u_view_projection;

out VS_OUT {
    vec2 uv;
} vs_out;

void main() {
    vs_out.uv = a_uv;
    gl_Position = u_view_projection * vec4(a_position, 1.0);
}
