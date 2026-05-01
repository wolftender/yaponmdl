#version 400

in VS_OUT {
    vec2 uv;
} fs_in;

uniform sampler2D u_texture;

out vec4 frag_color;

void main() {
    frag_color = vec4(texture(u_texture, fs_in.uv).rgba);
}
