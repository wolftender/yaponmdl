#version 400

in VS_OUT {
    vec2 uv;
    vec3 color;
} fs_in;

uniform sampler2D u_texture;
uniform vec4 u_color;
uniform vec2 u_uv_offset;
uniform vec2 u_uv_scale;
uniform float u_alpha;

out vec4 frag_color;

void main() {
    vec4 map_color = texture(u_texture, (fs_in.uv * u_uv_scale) + u_uv_offset).rgba;
    vec3 color = map_color.rgb * fs_in.color;

    frag_color = vec4(color.rgb, map_color.a * u_alpha) * u_color;
}
