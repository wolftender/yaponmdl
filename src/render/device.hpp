#pragma once
#include "common/fixedqueue.hpp"

#include "graphics/shader.hpp"
#include "render/model.hpp"

namespace render::hal {

class ICamera {
public:
    virtual auto GetProjection() const -> const glm::fmat4x4 & = 0;
    virtual auto GetView() const -> const glm::fmat4x4 & = 0;
};

class RenderDeviceOpenGL40 : public IDevice {
public:
    struct Description {
        std::string_view fs_static_source;
        std::string_view vs_static_source;
        std::string_view fs_skinned_source;
        std::string_view vs_skinned_source;
    };

    RenderDeviceOpenGL40(const gl::GLContext *context, const Description &description);
    ~RenderDeviceOpenGL40() = default;

    RenderDeviceOpenGL40(const RenderDeviceOpenGL40 &) = delete;
    auto operator=(const RenderDeviceOpenGL40 &) = delete;

    RenderDeviceOpenGL40(RenderDeviceOpenGL40 &&) = delete;
    auto operator=(RenderDeviceOpenGL40 &&) = delete;

    auto CreateSkinningBuffer() -> SkinningBufferHandle override;
    auto UpdateSkinningBuffer(SkinningBufferHandle &handle, std::span<const glm::fmat4x4> data) -> void override;
    auto UpdateSkinningBuffer(SkinningBufferHandle &handle, uint32_t id, const glm::fmat4x4 &matrix) -> void override;

    auto CreateMesh(std::span<const StaticVertex> vertices, std::span<const uint32_t> indices) -> MeshHandle override;
    auto CreateAnimatedMesh(std::span<const AnimatedVertex> vertices, std::span<const uint32_t> indices)
        -> AnimMeshHandle override;
    auto CreateRgbaTexture(const TextureDescription &desc) -> TextureHandle override;

    auto SubmitStaticDraw(const StaticDrawDescription &desc) -> void override;
    auto SubmitSkinnedDraw(const SkinnedDrawDescription &desc) -> void override;
    auto SubmitStaticDraw(StaticDrawDescription &&desc) -> void override;
    auto SubmitSkinnedDraw(SkinnedDrawDescription &&desc) -> void override;

    auto RenderFrame(const ICamera &camera) -> void;

private:
    const gl::GLContext *context_ = nullptr;

    gl::ShaderProgram static_shader_;
    gl::ShaderProgram skinned_shader_;

    util::FixedSizeQueue<StaticDrawDescription, 512> static_draws_;
    util::FixedSizeQueue<SkinnedDrawDescription, 256> skinned_draws_;
};

} // namespace render::hal
