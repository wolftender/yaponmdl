#pragma once
#include "formats/act.hpp"
#include "formats/gmo.hpp"
#include "formats/gxx.hpp"

#include "render/model.hpp"

namespace conv {

class ITextureRepository {
public:
    struct Bitmap {
        uint32_t width = 0;
        uint32_t height = 0;

        glm::fvec2 uv_scale = glm::fvec2{1.0f, 1.0f};
        glm::fvec2 uv_offset = glm::fvec2{0.0f, 0.0f};

        std::vector<uint8_t> plane;
    };

    virtual ~ITextureRepository() = default;
    virtual auto FetchTexture(std::string_view name) const -> std::optional<Bitmap> = 0;
};

class IConvertLogger {
public:
    virtual ~IConvertLogger() = default;
    virtual auto Log(std::string_view message) const -> void = 0;
};

auto ConvertGXX(
    const gxx::GxxModel &gxx_model, render::hal::IDevice *device, const ITextureRepository *repository = nullptr,
    const IConvertLogger *logger = nullptr) -> std::unique_ptr<render::Model>;

auto ConvertGMO(
    const gmo::GmoModel &gmo_model, render::hal::IDevice *device, const ITextureRepository *repository = nullptr,
    const IConvertLogger *logger = nullptr) -> std::unique_ptr<render::Model>;

auto ConvertACT(const act::Model &act_model, render::hal::IDevice *device, const IConvertLogger *logger = nullptr)
    -> std::unique_ptr<render::Model>;

} // namespace conv
