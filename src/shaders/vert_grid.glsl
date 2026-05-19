#version 400

layout(location = 0) in vec3 a_position;

out VS_OUT {
    vec3 near_point;
    vec3 far_point;
    vec2 uv;
} vs_out;

uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat4 u_view_inv;
uniform mat4 u_projection_inv;

vec3 unproject(vec3 position) {
    vec4 unprojected = u_view_inv * u_projection_inv * vec4(position.xyz, 1.0);
    return unprojected.xyz / unprojected.w;
}

void main() {
    vs_out.near_point = unproject(vec3(a_position.xy, 0.0));
    vs_out.far_point = unproject(vec3(a_position.xy, 1.0));
    vs_out.uv = (1.0 + a_position.xy) * 0.5;

    gl_Position = vec4(a_position, 1.0);
}
