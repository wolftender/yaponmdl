#version 400

layout(location = 0) in vec3 a_position;

out VS_OUT {
    vec2 uv;
} vs_out;

void main() {
    vs_out.uv = (1.0 + a_position.xy) * 0.5;
    gl_Position = vec4(a_position, 1.0);
}