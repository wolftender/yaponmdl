#include <cstdlib>
#include <fstream>
#include <vector>

#include <fmt/format.h>

#include "formats/gmo.hpp"
#include "formats/gmostring.hpp"

template <typename T> auto PrintVector(const T &v, size_t n) -> void {
    fmt::print("{}", '{');
    for (size_t i = 0; i < n; ++i) {
        fmt::print("{}", v[i]);
        if (i != n - 1) {
            fmt::print(", ");
        }
    }
    fmt::print("{} ", '}');
}

auto DebugPrintArray(const gmo::GmoVertexArray &array) -> void {
    fmt::println("vertex array");
    fmt::println("  array flags: ");

    const auto has_positions =
        (array.flags & gmo::GmoVertexArrayFlags::eHasPositions) == gmo::GmoVertexArrayFlags::eHasPositions;
    const auto has_normals =
        (array.flags & gmo::GmoVertexArrayFlags::eHasNormals) == gmo::GmoVertexArrayFlags::eHasNormals;
    const auto has_uvs = (array.flags & gmo::GmoVertexArrayFlags::eHasUvs) == gmo::GmoVertexArrayFlags::eHasUvs;
    const auto has_weights =
        (array.flags & gmo::GmoVertexArrayFlags::eHasWeights) == gmo::GmoVertexArrayFlags::eHasWeights;
    const auto has_color = (array.flags & gmo::GmoVertexArrayFlags::eHasNormals) == gmo::GmoVertexArrayFlags::eHasColor;

    if (has_positions) {
        fmt::println("    eHasPositions");
    }

    if (has_normals) {
        fmt::println("    eHasNormals");
    }

    if (has_uvs) {
        fmt::println("    eHasUvs");
    }

    if (has_weights) {
        fmt::println("    eHasWeights");
    }

    if (has_color) {
        fmt::println("    eHasColor");
    }

    fmt::println("  num weights: {}", array.num_weights);
    fmt::println("  vertices:");
    for (const auto &vertex : array.vertices) {
        fmt::print("    ");
        if (has_uvs) {
            PrintVector(vertex.uv, 2);
        }

        if (has_color) {
            PrintVector(vertex.color, 4);
        }

        if (has_normals) {
            PrintVector(vertex.normal, 3);
        }

        if (has_positions) {
            PrintVector(vertex.position, 3);
        }

        if (has_weights) {
            PrintVector(vertex.weights, array.num_weights);
        }
        fmt::print("\n");
    }
}

auto DebugPrintArrays(const gmo::GmoModel &model) -> void {
    for (const auto &part : model.parts) {
        for (const auto &array : part.vertex_arrays) {
            DebugPrintArray(array);
        }
    }
}

auto DebugPrintFcurves(const gmo::GmoModel &model) -> void {
    for (const auto &motion : model.motions) {
        fmt::println("motion {}", motion.name);
        fmt::println("  frame loop: {} - {}", motion.frame_loop_start, motion.frame_loop_end);
        fmt::println("  framerate: {}", motion.framerate);

        for (const auto &anim : motion.animations) {
            fmt::println(
                "    anim n={} prop={} t={} tid={}", anim.name, gmo::ToString(anim.property),
                gmo::ToString(anim.target), anim.target_id);
        }

        for (const auto &fcurve : motion.fcurves) {
            fmt::println(
                "    fcurve n={} dim={}, int={}, frames={}", fcurve.name, fcurve.dimensions,
                gmo::ToString(fcurve.interpolation), fcurve.num_keyframes);

            constexpr std::array<uint32_t, 5> kNumElements = {1, 1, 3, 5, 1};
            const auto stride = kNumElements[static_cast<uint32_t>(fcurve.interpolation)] * fcurve.dimensions + 1;

            uint32_t i = 0;
            for (uint32_t f = 0; f < fcurve.num_keyframes; ++f) {
                fmt::print("      ");
                for (uint32_t d = 0; d < stride; ++d) {
                    fmt::print("{} ", fcurve.raw_data[i++]);
                }
                fmt::print("\n");
            }
        }
    }
}

auto LoadBinaryFile(const std::string &path) -> std::vector<uint8_t> {
    std::fstream fs{path, std::ios::binary | std::ios::in};
    if (!fs.good()) {
        throw std::runtime_error{fmt::format("failed to open {}", path)};
    }

    fs.seekg(0, std::ios::end);
    const size_t size = fs.tellg();
    fs.seekg(0, std::ios::beg);

    std::vector<uint8_t> data;
    data.resize(size);
    fs.read(reinterpret_cast<char *>(data.data()), size);

    return data;
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
    const auto model_data = LoadBinaryFile("samples/chr50_04_01_1.gxx");
    const auto image_data = LoadBinaryFile("samples/chr50_04_01_1.gxt");

    try {
        const auto model = gmo::LoadModelFromMemory(model_data);
        fmt::println("done: {}", model.size());

        DebugPrintArrays(model.front());

        std::fstream ofs{"texture.bin", std::ios::binary | std::ios::out};
        const auto &tex = model.front().textures[0].data;
        ofs.write(reinterpret_cast<const char *>(tex.data()), tex.size());

        fmt::println("written {} bytes", tex.size());
    } catch (const std::exception &e) {
        fmt::println(stderr, "fatal error: {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
