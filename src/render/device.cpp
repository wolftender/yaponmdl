#include <fmt/format.h>

#include "render/device.hpp"

namespace render::hal {

auto RenderDeviceOpenGL40::MakeScreenQuad(gl::GLContext::Executor executor) -> gl::Mesh<PositionVertex> {
    const std::array<PositionVertex, 6> quad_verts = {
        PositionVertex{-1.0f, +1.0f, +0.0f}, PositionVertex{+1.0f, +1.0f, +0.0f}, PositionVertex{+1.0f, -1.0f, +0.0f},
        PositionVertex{-1.0f, +1.0f, +0.0f}, PositionVertex{+1.0f, -1.0f, +0.0f}, PositionVertex{-1.0f, -1.0f, +0.0f},
    };

    return gl::Mesh<PositionVertex>{executor, quad_verts};
}

RenderDeviceOpenGL40::RenderDeviceOpenGL40(const gl::GLContext *context, const Description &description)
    : context_{context},
      static_shader_{context->GetExecutor(), description.vs_static_source, description.fs_static_source},
      skinned_shader_{context->GetExecutor(), description.vs_skinned_source, description.fs_skinned_source},
      post_shader_{context->GetExecutor(), description.vs_post_filter, description.fs_post_filter},
      screen_quad_{MakeScreenQuad(context->GetExecutor())} {
    RebuildFramebuffers(description.target_width, description.target_height);
}

auto RenderDeviceOpenGL40::CreateSkinningBuffer() -> SkinningBufferHandle {
    GL_IMPLEMENTATION_INTERNAL;

    const auto id = skinning_pool_.Insert(
        gl::UniformBuffer<cbModelSkinningBuffer>{context_->GetExecutor(), cbModelSkinningBuffer{}});
    return CreateSkinningBufferHandle(id);
}

auto RenderDeviceOpenGL40::UpdateSkinningBuffer(SkinningBufferHandle &handle, std::span<const glm::fmat4x4> data)
    -> void {
    GL_IMPLEMENTATION_INTERNAL;

    auto *buffer = skinning_pool_.Get(handle.index());
    if (!buffer) {
        throw std::runtime_error{"invalid skinning buffer supplied for update"};
    }

    assert(data.size() <= kMaxMatricesPerSkin);
    std::copy(data.begin(), data.end(), buffer->GetData().matrices);
}

auto RenderDeviceOpenGL40::UpdateSkinningBuffer(SkinningBufferHandle &handle, uint32_t id, const glm::fmat4x4 &matrix)
    -> void {
    GL_IMPLEMENTATION_INTERNAL;

    auto *buffer = skinning_pool_.Get(handle.index());
    if (!buffer) {
        throw std::runtime_error{"invalid skinning buffer supplied for update"};
    }

    assert(id < kMaxMatricesPerSkin);
    buffer->GetData().matrices[id] = matrix;
}

auto RenderDeviceOpenGL40::CreateMesh(std::span<const StaticVertex> vertices, std::span<const uint32_t> indices)
    -> MeshHandle {
    GL_IMPLEMENTATION_INTERNAL;
    return CreateMeshHandle(mesh_pool_.Insert(gl::Mesh<StaticVertex>{context_->GetExecutor(), vertices, indices}));
}

auto RenderDeviceOpenGL40::CreateAnimatedMesh(
    std::span<const AnimatedVertex> vertices, std::span<const uint32_t> indices) -> AnimMeshHandle {
    GL_IMPLEMENTATION_INTERNAL;
    return CreateAnimMeshHandle(
        anim_mesh_pool_.Insert(gl::Mesh<AnimatedVertex>{context_->GetExecutor(), vertices, indices}));
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

    return CreateTextureHandle(
        texture_pool_.Insert(gl::Texture{context_->GetExecutor(), desc.width, desc.height, desc.data, parameters}));
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

    GL_CHECK(glDisable(GL_CULL_FACE));
    GL_CHECK(glEnable(GL_DEPTH_TEST));

    color_pass_fb_->Use([&]() {
        color_pass_fb_->ClearColorDepth(glm::fvec4{0.207f, 0.36f, 0.64f, 1.0f});
        GeometryPass(camera);
    });

    GL_CHECK(glDisable(GL_DEPTH_TEST));

    post_shader_.Use([&](const gl::ShaderProgram::Context &context) {
        color_target_->Bind(0);
        context.SetSampler("u_texture", 0);
        screen_quad_.Draw();
    });

    static_draws_.Clear();
    skinned_draws_.Clear();
}

auto RenderDeviceOpenGL40::ResizeFrame(uint32_t width, uint32_t height) -> void { RebuildFramebuffers(width, height); }

auto RenderDeviceOpenGL40::GeometryPass(const ICamera &camera) -> void {
    GL_IMPLEMENTATION_INTERNAL;

    static_shader_.Use([&](const gl::ShaderProgram::Context &context) {
        context.SetUniform("u_view", camera.GetView());
        context.SetUniform("u_projection", camera.GetProjection());
        context.SetUniform("u_diffuse", 0);

        for (uint32_t draw_id = 0; draw_id < static_draws_.GetFill(); ++draw_id) {
            const auto &draw = static_draws_[draw_id];
            const auto *mesh = mesh_pool_.Get(draw->mesh.index());

            if (!mesh) {
                throw std::runtime_error{"invalid mesh id supplied for draw"};
            }

            if (draw->diffuse_map.has_value()) {
                const auto *texture = texture_pool_.Get(draw->diffuse_map->index());
                if (texture) {
                    context.SetSampler("u_diffuse", 0);
                    texture->Bind(0);
                }
            }

            context.SetUniform("u_world", draw->world_matrix);
            mesh->Draw();

            GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
        }
    });

    skinned_shader_.Use([&](const gl::ShaderProgram::Context &context) {
        context.SetUniform("u_view", camera.GetView());
        context.SetUniform("u_projection", camera.GetProjection());
        context.SetUniform("u_diffuse", 0);

        for (uint32_t draw_id = 0; draw_id < skinned_draws_.GetFill(); ++draw_id) {
            const auto &draw = skinned_draws_[draw_id];
            const auto *mesh = anim_mesh_pool_.Get(draw->mesh.index());
            auto *skin_buffer = skinning_pool_.Get(draw->skinning_buffer.index());

            if (!mesh) {
                throw std::runtime_error{"invalid mesh id supplied for draw"};
            }

            if (draw->diffuse_map.has_value()) {
                const auto *texture = texture_pool_.Get(draw->diffuse_map->index());
                if (texture) {
                    context.SetSampler("u_diffuse", 0);
                    texture->Bind(0);
                }
            }

            skin_buffer->ExecuteUpload();

            context.SetBufferBase("u_bones", *skin_buffer);
            context.SetUniform("u_world", draw->world_matrix);
            mesh->Draw();

            GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
        }
    });
}

auto RenderDeviceOpenGL40::RebuildFramebuffers(uint32_t width, uint32_t height) -> void {
    GL_IMPLEMENTATION_INTERNAL;

    color_target_ = std::make_shared<gl::Texture>(
        context_->GetExecutor(), width, height, 1, gl::Texture::Parameters{gl::TextureFormat::eRGBA8});
    depth_target_ = std::make_shared<gl::Texture>(
        context_->GetExecutor(), width, height, 1, gl::Texture::Parameters{gl::TextureFormat::eDepthStencil});

    gl::Framebuffer::Description fb_description;
    fb_description.color_attachments[0] = color_target_;
    // fb_description.depth_attachment = depth_target_;

    color_pass_fb_.emplace(gl::Framebuffer{context_->GetExecutor(), width, height, fb_description});
}

} // namespace render::hal
