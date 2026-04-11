#pragma once
#include "formats/act.hpp"
#include "formats/gmo.hpp"

#include "render/model.hpp"

namespace conv {

class ITextureRepository {
public:
    struct Bitmap {
        uint32_t width;
        uint32_t height;

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

auto ConvertGMO(
    const gmo::GmoModel &gmo_model, render::hal::IDevice *device, const ITextureRepository *repository = nullptr,
    const IConvertLogger *logger = nullptr) -> std::unique_ptr<render::Model>;

auto ConvertACT(const act::Model &act_model, render::hal::IDevice *device, const IConvertLogger *logger = nullptr)
    -> std::unique_ptr<render::Model>;

} // namespace conv
