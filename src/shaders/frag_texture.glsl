#version 400

in VS_OUT {
    vec2 uv;
} fs_in;

uniform sampler2D u_texture;

out vec4 frag_color;

void main() {
    vec2 uv = vec2(fs_in.uv.x, 1.0 - fs_in.uv.y);
    frag_color = vec4(texture(u_texture, uv).rgba);
}
