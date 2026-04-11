#version 400

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec3 a_color;
layout(location = 3) in vec3 a_normal;
layout(location = 4) in vec4 a_weight;
layout(location = 5) in uvec4 a_bone;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_world;

out VS_OUT {
    vec2 uv;
    vec3 color;
} vs_out;

const int kMaxSkinnedBones = 200;
layout (std140) uniform u_bones {
    mat4 bone_matrix[kMaxSkinnedBones];
};

void main() {
    // calculate skinned position
    mat4 skin_matrix =
        bone_matrix[a_bone.x] * a_weight.x + 
        bone_matrix[a_bone.y] * a_weight.y +
        bone_matrix[a_bone.z] * a_weight.z +
        bone_matrix[a_bone.w] * a_weight.w;
    
    vs_out.uv = a_uv;
    vs_out.color = a_color;

    gl_Position = 
        u_projection * 
        u_view * 
        u_world * 
        skin_matrix * 
        vec4(a_position, 1.0);
}
