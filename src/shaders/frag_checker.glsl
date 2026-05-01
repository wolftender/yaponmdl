#version 400

in VS_OUT {
    vec2 uv;
} fs_in;

out vec4 frag_color;

uniform float u_divisions;
uniform float u_aspect;

uniform vec3 u_color1;
uniform vec3 u_color2;

void main() {
    const float pi = 3.14159265359;

    vec2 auv = vec2(u_aspect * fs_in.uv.x, 1.0 - fs_in.uv.y);
    vec2 suv = step(0.0,sin(u_divisions * pi * auv));
    
    float f = suv.x + suv.y;
    float v = f * (2.0 - f);

    vec3 gc = v * u_color1 + (1 - v) * u_color2;
    frag_color = vec4(gc, 1.0);
}
