#version 400

in VS_OUT {
    vec3 near_point;
    vec3 far_point;
    vec2 uv;
} fs_in;

out vec4 frag_color;

uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat4 u_view_inv;
uniform mat4 u_projection_inv;
uniform float u_grid_scale;

vec4 grid(vec3 frag_pos, float scale) {
    vec2 grid_plane_coord = frag_pos.xz * scale;
    vec2 dc = fwidth(grid_plane_coord);

    vec2 grid = abs(fract(grid_plane_coord - 0.5) - 0.5) / dc;
    float lc = min(grid.x, grid.y);

    float min_z = min(dc.y, 1.0);
    float min_x = min(dc.x, 1.0);
    vec4 color = vec4(0.3f, 0.3f, 0.3f, 1.0f - min(lc, 1.0f));

    float axis_width = 0.5f / u_grid_scale;
    if (frag_pos.x > -axis_width * min_x && frag_pos.x < axis_width * min_x) {
        color.x = 0.0;
        color.y = 0.0;
        color.z = 1.0;
    }
    
    if (frag_pos.z > -axis_width * min_z && frag_pos.z < axis_width * min_z) {
        color.x = 1.0;
        color.y = 0.0;
        color.z = 0.0;
    }
    
    return color;
}

float calc_depth(vec3 world_pos) {
    vec4 clip_pos = u_projection * u_view * vec4(world_pos, 1.0);
    return (clip_pos.z / clip_pos.w) * 0.5 + 0.5; // remapping is needed because OpenGL uses NDC [-1, 1]
}

void main() {
    float t = -fs_in.near_point.y / (fs_in.far_point.y - fs_in.near_point.y);
    float visiblity = step(0.0, t);

    vec3 frag_pos = fs_in.near_point + t * (fs_in.far_point - fs_in.near_point);
    
    frag_color = grid(frag_pos, u_grid_scale) * visiblity;
    frag_color.a = frag_color.a * (1.0 - 0.025 * length(frag_pos.xz) * sqrt(u_grid_scale));

    gl_FragDepth = calc_depth(frag_pos);
}