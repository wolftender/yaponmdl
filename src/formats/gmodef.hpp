#pragma once
#include <cstdint>

namespace gmo {

static constexpr uint32_t kSceGmoFormatSignature = 0x2e474d4f; // '.GMO'
static constexpr uint32_t kSceGmoFormatVersion = 0x312e3030;   // '1.00'
static constexpr uint32_t kSceGmoFormatPsp = 0x00505350;       // 'PSP'

// clang-format off

/**
 *   | offset | size | desc
 *   | 0      | 4    | primitive type (SCEGU_PRIM_*)
 *   | 4      | 4    | unused
 *   | 8      | 1    | is sequential
 *   | 9      | 3    | unused
 *   | 12     | 2    | spline u flags (SCEGU_CLOSE_* and SCEGU_OPEN_*)
 *   | 14     | 2    | spline v flags (SCEGU_CLOSE_* and SCEGU_OPEN_*)
 *   | 16     | 2    | unused
 *   | 18     | 3    | stride1
 *   | 21     | 3    | unused
 *   | 24     | 8    | stride2
 * 
 * stride is decoded as stride = stride1 * stride2
 * so can be decoded as stride = ((flags>>18)&7) * (flags>>24)
 */
enum GmoPrimitiveFlags : uint32_t {
    SCEGMO_PRIM_TYPE_MASK       = 0x000f, // 0000 0000 0000 1111
    SCEGMO_PRIM_POINTS          = 0x0000, // 0000 0000 0000 0000
    SCEGMO_PRIM_LINES           = 0x0001, // 0000 0000 0000 0001
    SCEGMO_PRIM_LINE_STRIP      = 0x0002, // 0000 0000 0000 0010
    SCEGMO_PRIM_TRIANGLES       = 0x0003, // 0000 0000 0000 0011
    SCEGMO_PRIM_TRIANGLE_STRIP  = 0x0004, // 0000 0000 0000 0100
    SCEGMO_PRIM_TRIANGLE_FAN    = 0x0005, // 0000 0000 0000 0101
    SCEGMO_PRIM_RECTANGLES      = 0x0006, // 0000 0000 0000 0110

    SCEGMO_PRIM_SPLINE_MASK     = 0xf000, // 1111 0000 0000 0000
    SCEGMO_PRIM_OPEN_U          = 0x3000, // 0011 0000 0000 0000
    SCEGMO_PRIM_OPEN_V          = 0xc000, // 1100 0000 0000 0000
    SCEGMO_PRIM_OPEN_U_IN       = 0x1000, // 0001 0000 0000 0000
    SCEGMO_PRIM_OPEN_U_OUT      = 0x2000, // 0010 0000 0000 0000
    SCEGMO_PRIM_OPEN_V_IN       = 0x4000, // 0100 0000 0000 0000
    SCEGMO_PRIM_OPEN_V_OUT      = 0x8000, // 1000 0000 0000 0000

    SCEGMO_PRIM_SEQUENTIAL      = 0x0100, // 0000 0001 0000 0000
};
// clang-format on

enum GmoChunkType : uint32_t {
    SCEGMO_BASE_RESERVED = 0x0000, /* 0000-0fff : reserved */
    SCEGMO_BASE_PRIVATE = 0x1000,  /* 1000-3fff : private use */
    SCEGMO_BASE_PUBLIC = 0x4000,   /* 4000-7fff : public use */

    SCEGMO_HALF_CHUNK = 0x8000, /* half chunk flag */

    SCEGMO_BLOCK = 0x0001,
    SCEGMO_FILE = 0x0002,
    SCEGMO_MODEL = 0x0003,
    SCEGMO_BONE = 0x0004,
    SCEGMO_PART = 0x0005,
    SCEGMO_MESH = 0x0006,
    SCEGMO_ARRAYS = 0x0007,
    SCEGMO_MATERIAL = 0x0008,
    SCEGMO_LAYER = 0x0009,
    SCEGMO_TEXTURE = 0x000a,
    SCEGMO_MOTION = 0x000b,
    SCEGMO_FCURVE = 0x000c,
    SCEGMO_BLIND_BLOCK = 0x000f,

    SCEGMO_COMMAND = 0x0011,
    SCEGMO_FILE_NAME = 0x0012,
    SCEGMO_FILE_IMAGE = 0x0013,
    SCEGMO_BOUNDING_BOX = 0x0014,
    SCEGMO_VERTEX_OFFSET = 0x0015,

    SCEGMO_PARENT_BONE = 0x0041,
    SCEGMO_VISIBILITY = 0x0042,
    SCEGMO_MORPH_WEIGHTS = 0x0043,
    SCEGMO_MORPH_INDEX = 0x004f,
    SCEGMO_BLEND_BONES = 0x0044,
    SCEGMO_BLEND_OFFSETS = 0x0045,
    SCEGMO_PIVOT = 0x0046,
    SCEGMO_MULT_MATRIX = 0x0047,
    SCEGMO_TRANSLATE = 0x0048,
    SCEGMO_ROTATE_ZYX = 0x0049,
    SCEGMO_ROTATE_YXZ = 0x004a,
    SCEGMO_ROTATE_Q = 0x004b,
    SCEGMO_SCALE = 0x004c,
    SCEGMO_SCALE_2 = 0x004d,
    SCEGMO_SCALE_3 = 0x00e1,
    SCEGMO_DRAW_PART = 0x004e,

    SCEGMO_SET_MATERIAL = 0x0061,
    SCEGMO_BLEND_SUBSET = 0x0062,
    SCEGMO_SUBDIVISION = 0x0063,
    SCEGMO_KNOT_VECTOR_U = 0x0064,
    SCEGMO_KNOT_VECTOR_V = 0x0065,
    SCEGMO_DRAW_ARRAYS = 0x0066,
    SCEGMO_DRAW_PARTICLE = 0x0067,
    SCEGMO_DRAW_B_SPLINE = 0x0068,
    SCEGMO_DRAW_RECT_MESH = 0x0069,
    SCEGMO_DRAW_RECT_PATCH = 0x006a,

    SCEGMO_RENDER_STATE = 0x0081,
    SCEGMO_DIFFUSE = 0x0082,
    SCEGMO_SPECULAR = 0x0083,
    SCEGMO_EMISSION = 0x0084,
    SCEGMO_AMBIENT = 0x0085,
    SCEGMO_REFLECTION = 0x0086,
    SCEGMO_REFRACTION = 0x0087,
    SCEGMO_BUMP = 0x0088,

    SCEGMO_SET_TEXTURE = 0x0091,
    SCEGMO_MAP_TYPE = 0x0092,
    SCEGMO_MAP_FACTOR = 0x0093,
    SCEGMO_BLEND_FUNC = 0x0094,
    SCEGMO_TEX_FUNC = 0x0095,
    SCEGMO_TEX_FILTER = 0x0096,
    SCEGMO_TEX_WRAP = 0x0097,
    SCEGMO_TEX_CROP = 0x0098,

    SCEGMO_FRAME_LOOP = 0x00b1,
    SCEGMO_FRAME_RATE = 0x00b2,
    SCEGMO_FRAME_REPEAT = 0x00b4,
    SCEGMO_ANIMATE = 0x00b3,

    SCEGMO_BLIND_DATA = 0x00f1,
    SCEGMO_FILE_INFO = 0x00ff
};

enum GmoFcurveFlags : uint32_t {
    SCEGMO_FCURVE_INTERP_MASK = 0x000f,
    SCEGMO_FCURVE_CONSTANT = 0x0000,
    SCEGMO_FCURVE_LINEAR = 0x0001,
    SCEGMO_FCURVE_HERMITE = 0x0002,
    SCEGMO_FCURVE_CUBIC = 0x0003,
    SCEGMO_FCURVE_SPHERICAL = 0x0004,

    SCEGMO_FCURVE_EXTRAP_MASK = 0xff00,
    SCEGMO_FCURVE_HOLD = 0x0000,
    SCEGMO_FCURVE_CYCLE = 0x1100,
    SCEGMO_FCURVE_SHUTTLE = 0x2200,
    SCEGMO_FCURVE_REPEAT = 0x3300,
    SCEGMO_FCURVE_EXTEND = 0x4400,

    SCEGMO_FCURVE_EXTRAP_LEFT_MASK = 0x0f00,
    SCEGMO_FCURVE_HOLD_LEFT = 0x0000,
    SCEGMO_FCURVE_CYCLE_LEFT = 0x0100,
    SCEGMO_FCURVE_SHUTTLE_LEFT = 0x0200,
    SCEGMO_FCURVE_REPEAT_LEFT = 0x0300,
    SCEGMO_FCURVE_EXTEND_LEFT = 0x0400,

    SCEGMO_FCURVE_EXTRAP_RIGHT_MASK = 0xf000,
    SCEGMO_FCURVE_HOLD_RIGHT = 0x0000,
    SCEGMO_FCURVE_CYCLE_RIGHT = 0x1000,
    SCEGMO_FCURVE_SHUTTLE_RIGHT = 0x2000,
    SCEGMO_FCURVE_REPEAT_RIGHT = 0x3000,
    SCEGMO_FCURVE_EXTEND_RIGHT = 0x4000,
};

enum GmoRepeatFlags : uint32_t {
    SCEGMO_REPEAT_HOLD = 0x0000,
    SCEGMO_REPEAT_CYCLE = 0x0001,
};

enum StateFlags : uint32_t {
    SCEGMO_STATE_LIGHTING = 0,
    SCEGMO_STATE_FOG = 1,
    SCEGMO_STATE_TEXTURE = 2,
    SCEGMO_STATE_CULL_FACE = 3,
    SCEGMO_STATE_DEPTH_TEST = 4,
    SCEGMO_STATE_DEPTH_MASK = 5,
    SCEGMO_STATE_ALPHA_TEST = 6,
    SCEGMO_STATE_ALPHA_MASK = 7,
};

enum GmoBlendFlags : uint32_t {
    SCEGMO_BLEND_ADD = 0,
    SCEGMO_BLEND_SUB = 1,
    SCEGMO_BLEND_REV = 2,
    SCEGMO_BLEND_MIN = 3,
    SCEGMO_BLEND_MAX = 4,
    SCEGMO_BLEND_DIFF = 5,

    SCEGMO_BLEND_ZERO = 0,
    SCEGMO_BLEND_ONE = 1,
    SCEGMO_BLEND_SRC_COLOR = 2,
    SCEGMO_BLEND_INV_SRC_COLOR = 3,
    SCEGMO_BLEND_DST_COLOR = 4,
    SCEGMO_BLEND_INV_DST_COLOR = 5,
    SCEGMO_BLEND_SRC_ALPHA = 6,
    SCEGMO_BLEND_INV_SRC_ALPHA = 7,
    SCEGMO_BLEND_DST_ALPHA = 8,
    SCEGMO_BLEND_INV_DST_ALPHA = 9,
};

enum GmoFuncFlags : uint32_t {
    SCEGMO_FUNC_MODULATE = 0,
    SCEGMO_FUNC_DECAL = 1,

    SCEGMO_FUNC_RGB = 0,
    SCEGMO_FUNC_RGBA = 1,
};

enum GmoFilterFlags : uint32_t {
    SCEGMO_FILTER_NEAREST = 0,
    SCEGMO_FILTER_LINEAR = 1,
    SCEGMO_FILTER_NEAREST_MIPMAP_NEAREST = 2,
    SCEGMO_FILTER_LINEAR_MIPMAP_NEAREST = 3,
    SCEGMO_FILTER_NEAREST_MIPMAP_LINEAR = 4,
    SCEGMO_FILTER_LINEAR_MIPMAP_LINEAR = 5,
};

enum GmoWrapFlags : uint32_t {
    SCEGMO_WRAP_REPEAT = 0,
    SCEGMO_WRAP_CLAMP = 1,
};

enum GmoBoneAnimFlags : uint32_t {
    SCEGMO_BONE_ANIM_MODIFIED = 0x0000ffff,
    SCEGMO_BONE_ANIM_COMPLETE = 0xffff0000,
};

// clang-format off
// from libgu.h

/* Primitive Type */
#define SCEGU_PRIM_POINTS                  0
#define SCEGU_PRIM_LINES                   1
#define SCEGU_PRIM_LINE_STRIP              2
#define SCEGU_PRIM_TRIANGLES               3
#define SCEGU_PRIM_TRIANGLE_STRIP          4
#define SCEGU_PRIM_TRIANGLE_FAN            5
#define SCEGU_PRIM_RECTANGLES              6

/* Spline Parameter */
#define SCEGU_CLOSE_CLOSE                  0
#define SCEGU_OPEN_CLOSE                   1
#define SCEGU_CLOSE_OPEN                   2
#define SCEGU_OPEN_OPEN                    3

/* Sprite Flip Type */
#define SCEGU_NOFLIP                       0
#define SCEGU_FLIP_U                       1
#define SCEGU_FLIP_V                       2
#define SCEGU_FLIP_UV                      3
#define SCEGU_NOROTATE                     0
#define SCEGU_ROTATE_90                    1
#define SCEGU_ROTATE_180                   2
#define SCEGU_ROTATE_270                   3

/**
 *   | offset | size | desc
 *   | 0      | 2    | texture format 
 *   | 2      | 3    | color format 
 *   | 5      | 2    | normal format 
 *   | 7      | 2    | vertex format 
 *   | 9      | 2    | weight format 
 *   | 11     | 2    | index format
 *   | 13     | 1    | reserved
 *   | 14     | 4    | num weights
 *   | 18     | 4    | num vertex
 *   | 22     | 1    | reserved
 *   | 23     | 1    | is through
 */

#define SCEGU_TEXTURE_NONE       ( 0 <<  0 )
#define SCEGU_TEXTURE_UBYTE      ( 1 <<  0 )
#define SCEGU_TEXTURE_USHORT     ( 2 <<  0 )
#define SCEGU_TEXTURE_FLOAT      ( 3 <<  0 )

#define SCEGU_COLOR_NONE         ( 0 <<  2 )
#define SCEGU_COLOR_PF5650       ( 4 <<  2 )
#define SCEGU_COLOR_PF5551       ( 5 <<  2 )
#define SCEGU_COLOR_PF4444       ( 6 <<  2 )
#define SCEGU_COLOR_PF8888       ( 7 <<  2 )

#define SCEGU_NORMAL_NONE        ( 0 <<  5 )
#define SCEGU_NORMAL_BYTE        ( 1 <<  5 )
#define SCEGU_NORMAL_SHORT       ( 2 <<  5 )
#define SCEGU_NORMAL_FLOAT       ( 3 <<  5 )

#define SCEGU_VERTEX_NONE        ( 0 <<  7 )
#define SCEGU_VERTEX_BYTE        ( 1 <<  7 )
#define SCEGU_VERTEX_SHORT       ( 2 <<  7 )
#define SCEGU_VERTEX_FLOAT       ( 3 <<  7 )

#define SCEGU_WEIGHT_NONE        ( 0 <<  9 )
#define SCEGU_WEIGHT_UBYTE       ( 1 <<  9 )
#define SCEGU_WEIGHT_USHORT      ( 2 <<  9 )
#define SCEGU_WEIGHT_FLOAT       ( 3 <<  9 )

#define SCEGU_INDEX_NONE         ( 0 << 11 )
#define SCEGU_INDEX_UBYTE        ( 1 << 11 )
#define SCEGU_INDEX_USHORT       ( 2 << 11 )

#define SCEGU_WEIGHT_1           ( 0 << 14 )
#define SCEGU_WEIGHT_2           ( 1 << 14 )
#define SCEGU_WEIGHT_3           ( 2 << 14 )
#define SCEGU_WEIGHT_4           ( 3 << 14 )
#define SCEGU_WEIGHT_5           ( 4 << 14 )
#define SCEGU_WEIGHT_6           ( 5 << 14 )
#define SCEGU_WEIGHT_7           ( 6 << 14 )
#define SCEGU_WEIGHT_8           ( 7 << 14 )

#define SCEGU_VERTEX_1           ( 0 << 18 )
#define SCEGU_VERTEX_2           ( 1 << 18 )
#define SCEGU_VERTEX_3           ( 2 << 18 )
#define SCEGU_VERTEX_4           ( 3 << 18 )
#define SCEGU_VERTEX_5           ( 4 << 18 )
#define SCEGU_VERTEX_6           ( 5 << 18 )
#define SCEGU_VERTEX_7           ( 6 << 18 )
#define SCEGU_VERTEX_8           ( 7 << 18 )
#define SCEGU_THROUGH            ( 1 << 23 )
// clang-format on

} // namespace gmo
