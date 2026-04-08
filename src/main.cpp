#include <cstdlib>
#include <fstream>
#include <vector>

#include <fmt/format.h>

#include "formats/gmo.hpp"

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

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
    std::fstream fs{"samples/chr01_01_01_1.gxx.gmo"};
    if (!fs.good()) {
        fmt::println("failed to open model");
        return EXIT_FAILURE;
    }

    fs.seekg(0, std::ios::end);
    const size_t size = fs.tellg();
    fs.seekg(0, std::ios::beg);

    std::vector<uint8_t> data;
    data.resize(size);
    fs.read(reinterpret_cast<char *>(data.data()), size);

    try {
        const auto model = gmo::LoadModelFromMemory(data);
        fmt::println("done: {}", model.size());

        DebugPrintArrays(model.front());
    } catch (const std::exception &e) {
        fmt::println(stderr, "failed to load model: {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
