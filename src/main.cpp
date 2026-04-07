#include <cstdlib>
#include <fstream>
#include <vector>

#include <fmt/format.h>

#include "formats/gmo.hpp"

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
        fmt::println("done: {}", sizeof(model));
    } catch (const std::exception &e) {
        fmt::println(stderr, "failed to load model: {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
