#pragma once
#include "common/fixedqueue.hpp"
#include "common/genpool.hpp"

#include "graphics/shader.hpp"
#include "graphics/mesh.hpp"
#include "graphics/texture.hpp"
#include "graphics/buffer.hpp"
#include "graphics/framebuffer.hpp"

#include "render/render.hpp"

namespace render::hal {

class ICamera {
public:
    virtual auto GetProjection() const -> const glm::fmat4x4 & = 0;
    virtual auto GetView() const -> const glm::fmat4x4 & = 0;

    virtual auto GetProjectionInv() const -> const glm::fmat4x4 & = 0;
    virtual auto GetViewInv() const -> const glm::fmat4x4 & = 0;
};

class RenderDeviceOpenGL40 : public IDevice {
public:
    enum class RendererType {
        eBaseSceneRenderer,
    };

private:
    struct PositionVertex {
        glm::fvec3 position;

        PositionVertex(const glm::fvec3 &position) : position{position} {}
        PositionVertex(float x, float y, float z) : position{x, y, z} {}

        static auto GetLayout() -> std::span<const gl::LayoutElement> {
            static constexpr std::array<gl::LayoutElement, 1> kLayout{gl::LayoutElement{
                0, 3, gl::ShaderAttribType::eFloat, sizeof(PositionVertex), offsetof(PositionVertex, position)}};

            return kLayout;
        }
    };

    static auto MakeScreenQuad(gl::GLContext::Executor executor) -> gl::Mesh<PositionVertex>;

    class ISceneRenderer {
    public:
        virtual ~ISceneRenderer() noexcept = default;
        virtual auto Execute(const ICamera &camera) -> void = 0;
        virtual auto Resize(uint32_t width, uint32_t height) -> void = 0;
    };

    class BaseSceneRenderer final : public ISceneRenderer {
    public:
        BaseSceneRenderer(RenderDeviceOpenGL40 *device, uint32_t target_width, uint32_t target_height);
        ~BaseSceneRenderer() noexcept = default;

        auto Execute(const ICamera &camera) -> void override;
        auto Resize(uint32_t width, uint32_t height) -> void override;

    private:
        auto GeometryPass(const ICamera &camera) -> void;
        auto RebuildFramebuffers(uint32_t width, uint32_t height) -> void;

        RenderDeviceOpenGL40 *device_ = nullptr;

        gl::ShaderProgram static_shader_;
        gl::ShaderProgram skinned_shader_;
        gl::ShaderProgram grid_shader_;
        gl::ShaderProgram post_shader_;
        gl::Mesh<PositionVertex> screen_quad_;

        std::shared_ptr<gl::Texture> color_target_;
        std::shared_ptr<gl::Texture> depth_target_;
        std::optional<gl::MultisampleFramebuffer> color_pass_fb_;
    };

public:
    static constexpr uint32_t kNumMaxSkinningBuffers = 256;

    RenderDeviceOpenGL40(
        const gl::GLContext *context, RendererType renderer_type, uint32_t target_width, uint32_t target_height);
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
    auto ResizeFrame(uint32_t width, uint32_t height) -> void;

private:
    const gl::GLContext *context_ = nullptr;
    std::unique_ptr<ISceneRenderer> scene_renderer_;

    util::FixedSizeQueue<StaticDrawDescription, 512> static_draws_;
    util::FixedSizeQueue<SkinnedDrawDescription, 256> skinned_draws_;

    util::GenerationalPool<gl::Mesh<StaticVertex>> mesh_pool_;
    util::GenerationalPool<gl::Mesh<AnimatedVertex>> anim_mesh_pool_;
    util::GenerationalPool<gl::Texture> texture_pool_;
    util::GenerationalPool<gl::UniformBuffer<cbModelSkinningBuffer>> skinning_pool_;
};

} // namespace render::hal
