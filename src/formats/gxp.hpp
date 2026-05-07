#include <cstdint>
#include <string>
#include <span>
#include <vector>
#include <optional>

#include <glm/glm.hpp>

namespace gxp {

using SceGxpCommand = uint32_t;

struct SceGxpMotionFrame {
    uint32_t offset;

    ///////////////////////////////////////////////////
    uint32_t ge_buffer_offs; // + 0x00 - ge commands start at this offset
    ///////////////////////////////////////////////////

    uint32_t ge_buffer_size; // this field does not exist in the stuct but can be figured out
};

struct SceGxpMotion {
    uint32_t offset;

    ///////////////////////////////////////////////////
    uint32_t frame_offs; // + 0x00 - frame array pointer
    uint32_t num_frames; // + 0x04 - frame array size
    float framerate;     // + 0x08 - framerate in fps
    uint32_t reserved;   // + 0x0c - unknown purpose
    ///////////////////////////////////////////////////

    std::vector<SceGxpMotionFrame> frames;
};

struct SceGxpNode {
    uint32_t offset;

    ///////////////////////////////////////////////////
    uint32_t matrix_offs; // + 0x00 - cmd buf ptr + this will give the matrix
    uint32_t name_offs;   // + 0x04 - name pointer (null terminated str)
    ///////////////////////////////////////////////////

    std::string name;
};

struct SceGxpBuffer {
    uint32_t offset;

    ///////////////////////////////////////////////////
    uint32_t type;     // + 0x00 - type enum
    uint32_t size;     // + 0x04
    uint32_t capacity; // + 0x08
    uint32_t owner;    // + 0x0c - unknown purpose, reserved
    ///////////////////////////////////////////////////
};

struct SceGxpObject {
    uint32_t offset;

    ///////////////////////////////////////////////////
    uint32_t type;          // + 0x00 - type enum
    uint32_t name_offs;     // + 0x04 - name pointer (null terminated str)
    uint32_t children_offs; // + 0x08 - children array pointer
    uint32_t num_children;  // + 0x0c - number of children objects
    uint32_t motion_offs;   // + 0x10 - motion array pointer
    uint32_t num_motions;   // + 0x14 - number of motions
    uint32_t node_offs;     // + 0x18 - node array pointer
    uint32_t num_nodes;     // + 0x1c - number of nodes
    ///////////////////////////////////////////////////

    std::string name;
    std::vector<SceGxpObject> children;
    std::vector<SceGxpMotion> motions;
    std::vector<SceGxpNode> nodes;
};

struct SceGxpFile {
    ///////////////////////////////////////////////////
    uint32_t magic;        // + 0x00 - ".GXX"
    uint32_t version;      // + 0x04 - "1.00"
    uint32_t format;       // + 0x08 - either ".GMO" or ".GIM"
    uint32_t option;       // + 0x0c - always zero, most likely no purpose
    uint32_t root_offs;    // + 0x10 - root object pointer, should be the same as object_offs
    uint32_t buffer_ptr;   // + 0x14 - unknown
    uint32_t reserved0x18; // + 0x18 - unknown
    uint32_t reserved0x1c; // + 0x1c - padding
    uint32_t object_offs;  // + 0x20 - object offset (root object)
    uint32_t object_size;  // + 0x24 - object size
    uint32_t packet_offs;  // + 0x28 - packet offset (ge buffers are stored in the "packet")
    uint32_t packet_size;  // + 0x2c - packet size
    uint32_t vertex_offs;  // + 0x30 - vertex offset (vertex buffer section)
    uint32_t vertex_size;  // + 0x34 - vertex size
    uint32_t pixel_offs;   // + 0x38 - pixel offset (for gxt will store the image pixels)
    uint32_t pixel_size;   // + 0x3c - pixel size (zero for gxx)
    ///////////////////////////////////////////////////

    SceGxpObject root_object;
    std::optional<SceGxpBuffer> root_packet;
    std::optional<SceGxpBuffer> root_vertex;
    std::optional<SceGxpBuffer> root_pixel;
};

class GxpLogger {
public:
    virtual ~GxpLogger() = default;
    virtual auto log(std::string_view log_message) const -> void = 0;
};

auto LoadFromMemory(const std::span<const uint8_t> buffer, const GxpLogger *logger = nullptr) -> SceGxpFile;

} // namespace gxp
