#version 400

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec3 a_color;

// ignored for patapon meshes
// layout(location = 3) in vec3 a_normal;

// ignored attributes in static draws
// layout(location = 4) in vec4 a_weight;
// layout(location = 5) in uvec4 a_bone;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_world;

out VS_OUT {
    vec2 uv;
    vec3 color;
} vs_out;

void main() {
    vs_out.uv = a_uv;
    vs_out.color = a_color;
    gl_Position = u_projection * u_view * u_world * vec4(a_position, 1.0);
}
