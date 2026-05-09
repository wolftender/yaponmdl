struct SceGxpCommandBuffer {
    u32 commands;
};

struct SceGxpFrame {
   u32 command_offs;
   SceGxpCommandBuffer cmd_buf @ command_offs;
   u32 cmd_buf_size @ (command_offs - 0x38);
};

struct SceGxpMotion {
    u32 frame_offs;
    u32 num_frames;
    float framerate;
    u32 pad;
    
    if (num_frames > 0) {
        SceGxpFrame frames[num_frames] @ frame_offs;
    }
};

struct SceGxpNode {
    u32 matrix_offs;
    u32 name_offs;
    
    char name[] @ name_offs;
};

struct SceGxpBuffer {
    u32 type;
    u32 size;
    u32 capacity;
    u32 owner;
};

using SceGxpObject;

struct SceGxpObjectChild {
    u32 offset;
    SceGxpObject child @ offset;
};

struct SceGxpObject {
    u32 type;               // +0x00
    u32 name_offs;               // +0x04
    u32 children_arr_offs;      // +0x08
    u32 num_children;       // +0x0c
    u32 motion_offs;        // +0x10
    u32 num_motions;        // +0x14
    u32 bone_offs;          // +0x18
    u32 num_bones;          // +0x1c
    
    if (name_offs > 0) {
        char name[] @ name_offs; 
    }
    
    if (num_children > 0) {
        SceGxpObjectChild children[num_children] @ children_arr_offs;
    }
    
    if (num_motions > 0) {
        SceGxpMotion motions[num_motions] @ motion_offs;
    }
    
    if (num_bones > 0) {
        SceGxpNode bones[num_bones] @ bone_offs;
    }
};

struct SceGxpHeader {
    u32 magic;          // +0x00
    u32 version;        // +0x04
    u32 format;         // +0x08
    u32 option;         // +0x0c
    u32 root_offs;       // +0x10
    u32 buffer_ptr;
    u32 num_ref;
    u32 pad;
    u32 object_offs;
    u32 object_size;
    u32 packet_offs;
    u32 packet_size;
    u32 vertex_offs;
    u32 vertex_size;
    u32 pixel_offs;
    u32 pixel_size;
    
    if (object_size != 0) {
        SceGxpObject object @ object_offs;
    }
    
    if (packet_size != 0) {
        SceGxpBuffer packet @ packet_offs;
    }
    
    if (vertex_size != 0) {
        SceGxpBuffer vertex @ vertex_offs;
    }
    
    if (pixel_size != 0) {
        SceGxpBuffer pixel @ pixel_offs;
    }
};

SceGxpHeader gxp @ 0x00;

// parsing bones
//  1. take your desired bone
//  2. take its offset
//  3. get the current frames command list offset
//  4. add the matrix offset to the command list offset
//  5. you should have landed on a GE command with opcode 0x3b, congratulations
//  6. now just read 12 values and mask them with 0x00ffffff

fn bone_mat_in_frame(u32 motion_idx, u32 frame_idx, u32 bone_idx) {
    u32 mat_cmds[12] @ (
        gxp.object.motions[motion_idx]
        .frames[frame_idx]
        .command_offs + 
        gxp.object.bones[bone_idx].matrix_offs);
          
    for (auto i = 0, i < 12, i = i + 1) {
        std::print(mat_cmds[i]);
    }
};

u32 mat_00 @ (gxp.object.motions[0].frames[0].command_offs + gxp.object.bones[0].matrix_offs);
u32 mat_01 @ (gxp.object.motions[0].frames[0].command_offs + gxp.object.bones[1].matrix_offs);
u32 mat_02 @ (gxp.object.motions[0].frames[0].command_offs + gxp.object.bones[2].matrix_offs);
u32 mat_03 @ (gxp.object.motions[0].frames[0].command_offs + gxp.object.bones[3].matrix_offs);
u32 mat_04 @ (gxp.object.motions[0].frames[0].command_offs + gxp.object.bones[4].matrix_offs);
u32 mat_05 @ (gxp.object.motions[0].frames[0].command_offs + gxp.object.bones[5].matrix_offs);
u32 mat_06 @ (gxp.object.motions[0].frames[0].command_offs + gxp.object.bones[6].matrix_offs);
u32 mat_07 @ (gxp.object.motions[0].frames[0].command_offs + gxp.object.bones[7].matrix_offs);
