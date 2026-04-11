#include <fmt/format.h>

#include "render/device.hpp"

namespace render::hal {

RenderDeviceOpenGL40::RenderDeviceOpenGL40(const gl::GLContext *context, const Description &description)
    : context_{context},
      static_shader_{context->GetExecutor(), description.vs_static_source, description.fs_static_source},
      skinned_shader_{context->GetExecutor(), description.vs_skinned_source, description.fs_skinned_source} {}

auto RenderDeviceOpenGL40::CreateSkinningBuffer() -> SkinningBufferHandle {
    GL_IMPLEMENTATION_INTERNAL;

    gl::UniformBuffer<cbModelSkinningBuffer> skinning_buffer{context_->GetExecutor(), cbModelSkinningBuffer{}, 0};
    return skinning_buffer;
}

auto RenderDeviceOpenGL40::UpdateSkinningBuffer(SkinningBufferHandle &handle, std::span<const glm::fmat4x4> data)
    -> void {
    GL_IMPLEMENTATION_INTERNAL;

    assert(data.size() < kMaxMatricesPerSkin);
    std::copy(data.begin(), data.end(), handle.GetData().matrices);

    handle.Update();
}

auto RenderDeviceOpenGL40::UpdateSkinningBuffer(SkinningBufferHandle &handle, uint32_t id, const glm::fmat4x4 &matrix)
    -> void {
    GL_IMPLEMENTATION_INTERNAL;

    assert(id < kMaxMatricesPerSkin);
    handle.GetData().matrices[id] = matrix;
}

auto RenderDeviceOpenGL40::CreateMesh(std::span<const StaticVertex> vertices, std::span<const uint32_t> indices)
    -> MeshHandle {
    GL_IMPLEMENTATION_INTERNAL;
    gl::Mesh<StaticVertex> mesh{context_->GetExecutor(), vertices, indices};

    return mesh;
}

auto RenderDeviceOpenGL40::CreateAnimatedMesh(
    std::span<const AnimatedVertex> vertices, std::span<const uint32_t> indices) -> AnimMeshHandle {
    GL_IMPLEMENTATION_INTERNAL;
    gl::Mesh<AnimatedVertex> mesh{context_->GetExecutor(), vertices, indices};

    return mesh;
}

inline auto ToGLEnum(const TextureMinFilter &min_filter) -> gl::TextureMinFilter {
    switch (min_filter) {
    case render::hal::TextureMinFilter::eLinear:
        return gl::TextureMinFilter::eLinear;
    case render::hal::TextureMinFilter::eNearest:
        return gl::TextureMinFilter::eNearest;
    default:
        throw std::runtime_error{fmt::format("invalid min filter")};
    }
}

inline auto ToGLEnum(const TextureMagFilter &min_filter) -> gl::TextureMagFilter {
    switch (min_filter) {
    case render::hal::TextureMagFilter::eLinear:
        return gl::TextureMagFilter::eLinear;
    case render::hal::TextureMagFilter::eNearest:
        return gl::TextureMagFilter::eNearest;
    default:
        throw std::runtime_error{fmt::format("invalid mag filter")};
    }
}

inline auto ToGLEnum(const TextureWrap &min_filter) -> gl::TextureWrap {
    switch (min_filter) {
    case render::hal::TextureWrap::eRepeat:
        return gl::TextureWrap::eRepeat;
    case render::hal::TextureWrap::eClampEdge:
        return gl::TextureWrap::eClampToEdge;
    default:
        throw std::runtime_error{fmt::format("invalid mag filter")};
    }
}

auto RenderDeviceOpenGL40::CreateRgbaTexture(const TextureDescription &desc) -> TextureHandle {
    GL_IMPLEMENTATION_INTERNAL;

    gl::Texture::Parameters parameters = {
        gl::TextureFormat::eRGBA8, ToGLEnum(desc.min_filter), ToGLEnum(desc.mag_filter),
        ToGLEnum(desc.wrap_s),     ToGLEnum(desc.wrap_t),
    };

    return gl::Texture{context_->GetExecutor(), desc.width, desc.height, desc.data, parameters};
}

auto RenderDeviceOpenGL40::SubmitStaticDraw(const StaticDrawDescription &desc) -> void {
    static_draws_.Push(StaticDrawDescription{desc});
}

auto RenderDeviceOpenGL40::SubmitSkinnedDraw(const SkinnedDrawDescription &desc) -> void {
    skinned_draws_.Push(SkinnedDrawDescription{desc});
}

auto RenderDeviceOpenGL40::SubmitStaticDraw(StaticDrawDescription &&desc) -> void {
    static_draws_.Push(std::move(desc));
}

auto RenderDeviceOpenGL40::SubmitSkinnedDraw(SkinnedDrawDescription &&desc) -> void {
    skinned_draws_.Push(std::move(desc));
}

auto RenderDeviceOpenGL40::RenderFrame(const ICamera &camera) -> void {
    GL_IMPLEMENTATION_INTERNAL;

    static_shader_.Use([&](const gl::ShaderProgram::Context &context) {
        context.SetUniform("u_view", camera.GetView());
        context.SetUniform("u_projection", camera.GetProjection());
        context.SetUniform("u_diffuse", 0);

        for (uint32_t draw_id = 0; draw_id < static_draws_.GetFill(); ++draw_id) {
            const auto &draw = static_draws_[draw_id];

            if (draw->diffuse_map) {
                context.SetSampler("u_diffuse", 0);
                draw->diffuse_map->Bind(0);
            }

            context.SetUniform("u_world", draw->world_matrix);
            draw->mesh->Draw();
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
        }
    });

    skinned_shader_.Use([&](const gl::ShaderProgram::Context &context) {
        context.SetUniform("u_view", camera.GetView());
        context.SetUniform("u_projection", camera.GetProjection());
        context.SetUniform("u_diffuse", 0);

        for (uint32_t draw_id = 0; draw_id < skinned_draws_.GetFill(); ++draw_id) {
            const auto &draw = skinned_draws_[draw_id];

            if (draw->diffuse_map) {
                draw->diffuse_map->Bind(0);
                context.SetSampler("u_diffuse", 0);
            }

            context.SetUniform("u_world", draw->world_matrix);
            draw->mesh->Draw();
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
        }
    });

    static_draws_.Clear();
    skinned_draws_.Clear();
}

} // namespace render::hal
