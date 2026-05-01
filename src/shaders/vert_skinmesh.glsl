#version 400

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec3 a_color;
layout(location = 3) in vec3 a_normal;
layout(location = 4) in vec4 a_tangent;
layout(location = 5) in vec4 a_weight0;
layout(location = 6) in vec4 a_weight1;
layout(location = 7) in uvec4 a_bone0;
layout(location = 8) in uvec4 a_bone1;

uniform mat4 u_projection;
uniform mat4 u_view;
uniform mat4 u_world; // should be identity for models

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
        bone_matrix[a_bone0.x] * a_weight0.x + 
        bone_matrix[a_bone0.y] * a_weight0.y +
        bone_matrix[a_bone0.z] * a_weight0.z +
        bone_matrix[a_bone0.w] * a_weight0.w +
        bone_matrix[a_bone1.x] * a_weight1.x + 
        bone_matrix[a_bone1.y] * a_weight1.y +
        bone_matrix[a_bone1.z] * a_weight1.z +
        bone_matrix[a_bone1.w] * a_weight1.w;
    
    vs_out.uv = a_uv;
    vs_out.color = a_color;

    gl_Position = 
        u_projection * 
        u_view * 
        u_world * 
        skin_matrix * 
        vec4(a_position, 1.0);
}
