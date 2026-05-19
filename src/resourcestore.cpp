#include "resourcestore.hpp"

#include "generated/vert_texture.glsl.h"
#include "generated/frag_texture.glsl.h"
#include "generated/frag_checker.glsl.h"
#include "generated/frag_mesh.glsl.h"
#include "generated/vert_skinmesh.glsl.h"
#include "generated/vert_staticmesh.glsl.h"
#include "generated/vert_postfilter.glsl.h"
#include "generated/frag_postfilter.glsl.h"
#include "generated/vert_text.glsl.h"
#include "generated/frag_text.glsl.h"
#include "generated/vert_grid.glsl.h"
#include "generated/frag_grid.glsl.h"
#include "generated/opensans.ttf.h"

namespace memoryresource {

auto GetVertShaderTexture() -> std::string_view {
    return std::string_view{reinterpret_cast<const char *>(kVertTexture_glsl.data()), kVertTexture_glsl.size()};
}

auto GetFragShaderTexture() -> std::string_view {
    return std::string_view{reinterpret_cast<const char *>(kFragTexture_glsl.data()), kFragTexture_glsl.size()};
}

auto GetFragShaderChecker() -> std::string_view {
    return std::string_view{reinterpret_cast<const char *>(kFragChecker_glsl.data()), kFragChecker_glsl.size()};
}

auto GetFragShaderMesh() -> std::string_view {
    return std::string_view{reinterpret_cast<const char *>(kFragMesh_glsl.data()), kFragMesh_glsl.size()};
}

auto GetVertShaderStaticMesh() -> std::string_view {
    return std::string_view{reinterpret_cast<const char *>(kVertStaticmesh_glsl.data()), kVertStaticmesh_glsl.size()};
}

auto GetVertShaderSkinnedMesh() -> std::string_view {
    return std::string_view{reinterpret_cast<const char *>(kVertSkinmesh_glsl.data()), kVertSkinmesh_glsl.size()};
}

auto GetVertShaderPost() -> std::string_view {
    return std::string_view{reinterpret_cast<const char *>(kVertPostfilter_glsl.data()), kVertPostfilter_glsl.size()};
}

auto GetFragShaderPost() -> std::string_view {
    return std::string_view{reinterpret_cast<const char *>(kFragPostfilter_glsl.data()), kFragPostfilter_glsl.size()};
}

auto GetVertShaderText() -> std::string_view {
    return std::string_view{reinterpret_cast<const char *>(kVertText_glsl.data()), kVertText_glsl.size()};
}

auto GetFragShaderText() -> std::string_view {
    return std::string_view{reinterpret_cast<const char *>(kFragText_glsl.data()), kFragText_glsl.size()};
}

auto GetVertShaderGrid() -> std::string_view {
    return std::string_view{reinterpret_cast<const char *>(kVertGrid_glsl.data()), kVertGrid_glsl.size()};
}

auto GetFragShaderGrid() -> std::string_view {
    return std::string_view{reinterpret_cast<const char *>(kFragGrid_glsl.data()), kFragGrid_glsl.size()};
}

auto GetFontOpensansTtf() -> std::span<const uint8_t> { return kOpensans_ttf; }

} // namespace memoryresource
