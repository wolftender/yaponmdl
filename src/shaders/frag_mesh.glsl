#version 400

in VS_OUT {
    vec2 uv;
    vec3 color;
} fs_in;

uniform sampler2D u_texture;

out vec4 frag_color;

void main() {
    const float kAlphaCutoff = 0.5;

    frag_color = vec4(texture(u_texture, fs_in.uv).rgba);

    // https://bgolus.medium.com/anti-aliased-alpha-test-the-esoteric-alpha-to-coverage-8b177335ae4f
    frag_color.a = (frag_color.a - kAlphaCutoff) / max(fwidth(frag_color.a), 0.0001) + 0.5;
}
