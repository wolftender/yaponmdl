#pragma once
#include <array>
#include <optional>

#include <glm/glm.hpp>

#include "graphics/gl.hpp"

namespace render {

struct TexturedVertex {
    glm::fvec3 position;
    glm::fvec2 uv;
    glm::fvec3 color;

    static auto GetLayout() -> std::span<const gl::LayoutElement> {
        static constexpr std::array<gl::LayoutElement, 3> kLayout{
            gl::LayoutElement{
                0, 3, gl::ShaderAttribType::eFloat, sizeof(TexturedVertex), offsetof(TexturedVertex, position)},
            gl::LayoutElement{1, 2, gl::ShaderAttribType::eFloat, sizeof(TexturedVertex), offsetof(TexturedVertex, uv)},
            gl::LayoutElement{
                2, 3, gl::ShaderAttribType::eFloat, sizeof(TexturedVertex), offsetof(TexturedVertex, color)},
        };

        return kLayout;
    }
};

struct ModelVertex {
    glm::fvec3 position;
    glm::fvec2 uv;
    glm::fvec3 color;
    glm::fvec3 normal;
    glm::fvec4 tangent;
    float weights[8];
    uint32_t bones[8];

    static auto GetLayout() -> std::span<const gl::LayoutElement> {
        static constexpr std::array<gl::LayoutElement, 9> kLayout{
            gl::LayoutElement{0, 3, gl::ShaderAttribType::eFloat, sizeof(ModelVertex), offsetof(ModelVertex, position)},
            gl::LayoutElement{1, 2, gl::ShaderAttribType::eFloat, sizeof(ModelVertex), offsetof(ModelVertex, uv)},
            gl::LayoutElement{2, 3, gl::ShaderAttribType::eFloat, sizeof(ModelVertex), offsetof(ModelVertex, color)},
            gl::LayoutElement{3, 3, gl::ShaderAttribType::eFloat, sizeof(ModelVertex), offsetof(ModelVertex, normal)},
            gl::LayoutElement{4, 3, gl::ShaderAttribType::eFloat, sizeof(ModelVertex), offsetof(ModelVertex, tangent)},
            gl::LayoutElement{5, 4, gl::ShaderAttribType::eFloat, sizeof(ModelVertex), offsetof(ModelVertex, weights)},
            gl::LayoutElement{
                6, 4, gl::ShaderAttribType::eFloat, sizeof(ModelVertex), offsetof(ModelVertex, weights[4])},
            gl::LayoutElement{
                7, 4, gl::ShaderAttribType::eUnsignedInt, sizeof(ModelVertex), offsetof(ModelVertex, bones)},
            gl::LayoutElement{
                8, 4, gl::ShaderAttribType::eUnsignedInt, sizeof(ModelVertex), offsetof(ModelVertex, bones[4])},
        };

        return kLayout;
    }
};

constexpr uint32_t kMaxMatricesPerSkin = 200;

struct cbModelSkinningBuffer {
    glm::fmat4x4 matrices[kMaxMatricesPerSkin];
};

using StaticVertex = ModelVertex;
using AnimatedVertex = ModelVertex;

// handles for primitives
namespace hal {

enum class TextureMinFilter {
    eNearest,
    eLinear,
};

enum class TextureMagFilter {
    eNearest,
    eLinear,
};

enum class TextureWrap {
    eRepeat,
    eClampEdge,
};

struct TextureDescription {
    uint32_t width;
    uint32_t height;

    TextureMinFilter min_filter;
    TextureMagFilter mag_filter;

    TextureWrap wrap_s;
    TextureWrap wrap_t;

    std::span<const uint8_t> data;
};

class IDevice {
private:
    struct TextureTag;
    struct MeshTag;
    struct AnimMeshTag;
    struct SkinningBufferTag;

public:
    template <typename Tag> class Id {
    public:
        auto index() const -> uint64_t { return index_; }

        Id(const Id &) = default;
        auto operator=(const Id &) -> Id & = default;

    private:
        explicit Id(uint64_t index) : index_{index} {}
        uint64_t index_;

        friend class IDevice;
    };

    using TextureHandle = Id<TextureTag>;
    using MeshHandle = Id<MeshTag>;
    using AnimMeshHandle = Id<AnimMeshTag>;
    using SkinningBufferHandle = Id<SkinningBufferTag>;

    struct StaticDrawDescription {
        MeshHandle mesh;
        glm::fmat4x4 world_matrix = {1.0f};

        std::optional<TextureHandle> diffuse_map;
        std::optional<TextureHandle> normal_map;
    };

    struct SkinnedDrawDescription {
        AnimMeshHandle mesh;
        SkinningBufferHandle skinning_buffer;

        glm::fmat4x4 world_matrix = {1.0f};

        std::optional<TextureHandle> diffuse_map;
        std::optional<TextureHandle> normal_map;
    };

    IDevice() = default;
    virtual ~IDevice() noexcept = default;

    // we don't really want the device to move around in memory
    // because the model wants to keep a pointer to the device

    IDevice(const IDevice &) = delete;
    auto operator=(const IDevice &) = delete;

    IDevice(IDevice &&) = delete;
    auto operator=(IDevice &&) = delete;

    virtual auto CreateSkinningBuffer() -> SkinningBufferHandle = 0;
    virtual auto UpdateSkinningBuffer(SkinningBufferHandle &handle, std::span<const glm::fmat4x4> data) -> void = 0;
    virtual auto UpdateSkinningBuffer(SkinningBufferHandle &handle, uint32_t id, const glm::fmat4x4 &matrix)
        -> void = 0;

    virtual auto CreateMesh(std::span<const StaticVertex> vertices, std::span<const uint32_t> indices)
        -> MeshHandle = 0;
    virtual auto CreateAnimatedMesh(std::span<const AnimatedVertex> vertices, std::span<const uint32_t> indices)
        -> AnimMeshHandle = 0;
    virtual auto CreateRgbaTexture(const TextureDescription &desc) -> TextureHandle = 0;

    virtual auto SubmitStaticDraw(const StaticDrawDescription &desc) -> void = 0;
    virtual auto SubmitSkinnedDraw(const SkinnedDrawDescription &desc) -> void = 0;
    virtual auto SubmitStaticDraw(StaticDrawDescription &&desc) -> void = 0;
    virtual auto SubmitSkinnedDraw(SkinnedDrawDescription &&desc) -> void = 0;

protected:
    auto CreateTextureHandle(uint64_t value) const -> TextureHandle { return TextureHandle{value}; }
    auto CreateMeshHandle(uint64_t value) const -> MeshHandle { return MeshHandle{value}; }
    auto CreateAnimMeshHandle(uint64_t value) const -> AnimMeshHandle { return AnimMeshHandle{value}; }

    auto CreateSkinningBufferHandle(uint64_t value) const -> SkinningBufferHandle {
        return SkinningBufferHandle{value};
    }
};

using TextureHandle = IDevice::TextureHandle;
using MeshHandle = IDevice::MeshHandle;
using AnimMeshHandle = IDevice::AnimMeshHandle;
using SkinningBufferHandle = IDevice::SkinningBufferHandle;

using StaticDrawDescription = IDevice::StaticDrawDescription;
using SkinnedDrawDescription = IDevice::SkinnedDrawDescription;

} // namespace hal

} // namespace render
