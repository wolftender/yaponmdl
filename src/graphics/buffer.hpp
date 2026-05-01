#pragma once
#include <optional>

#include "graphics/gl.hpp"

/*
 * This is an ugly abstraction on uniform buffers in OpenGL,
 * a simple summary how it works to best explain how to use it:
 *
 * OpenGL glossary
 * - glBufferData is basically GPU malloc, there is no size limit
 * - glBufferSubData will not reallocate anything, it enforces sync
 * - glBindBufferBase binds the WHOLE buffer as UBO to given "ubo index"
 * - glBindBufferRange binds a buffer subrange as UBO to given "ubo index"
 * - glUniformBlockBinding points the glsl UBO to given "ubo index"
 * "ubo index" is just some OpenGL state machine concept, updating it is just
 * a CPU-side thing apparently
 *
 */

namespace gl {

enum class BufferType : GLenum {
    eVertexBuffer = GL_ARRAY_BUFFER,
    eIndexBuffer = GL_ELEMENT_ARRAY_BUFFER,
    eUniformBuffer = GL_UNIFORM_BUFFER,
};

enum class BufferUsage : GLenum {
    eStaticDraw = GL_STATIC_DRAW,
    eDynamicDraw = GL_DYNAMIC_DRAW,
};

enum class BufferMappingType : GLenum {
    eReadOnly = GL_READ_ONLY,
    eWriteOnly = GL_WRITE_ONLY,
    eReadWrite = GL_READ_WRITE,
};

class Buffer final {
public:
    struct Description {
        BufferType type;
        BufferUsage usage;

        size_t size;
        std::optional<std::span<const uint8_t>> init_data;
    };

    Buffer(GLContext::Executor &&executor, const Description &description);

    Buffer(const Buffer &) = delete;
    auto operator=(const Buffer &) = delete;

    Buffer(Buffer &&) = default;
    auto operator=(Buffer &&) -> Buffer & = default;

    auto GetExecutor() const -> const gl::GLContext::Executor & { return executor_; }
    auto GetHandle() const -> const BufferHandle & { return handle_; }
    auto GetInitialType() const -> BufferType { return initial_type_; }
    auto GetUsage() const -> BufferUsage { return usage_; }
    auto GetSize() const -> uint64_t { return size_; }

    auto GetMappedPointer() const -> void * { return cpu_mapped_; }

    template <std::invocable<void *> F> auto Map(BufferMappingType map_type, F consumer) {
        if (cpu_mapped_ != nullptr) {
            throw std::runtime_error{"buffer cannot be mapped twice"};
        }

        executor_.RunOnContext([&](const auto &) {
            GL_CHECK(glBindBuffer(type_gl_, handle_));
            cpu_mapped_ = GL_CHECK(glMapBuffer(type_gl_, static_cast<GLenum>(map_type)));
            consumer(cpu_mapped_);
            GL_CHECK(glUnmapBuffer(type_gl_));
            GL_CHECK(glBindBuffer(type_gl_, 0));
        });
    }

    auto Orphan(uint64_t offset = 0, std::optional<std::span<const uint8_t>> new_data = std::nullopt) -> void;
    auto Update(uint64_t offset, std::span<const uint8_t> new_data) -> void;

    template <typename T>
        requires(!std::convertible_to<T, std::span<const uint8_t>>)
    auto Orphan(uint64_t offset, const T &new_data) -> void {
        const auto new_data_mem = reinterpret_cast<const uint8_t *>(&new_data);
        const auto new_data_size = sizeof(T);

        Orphan(offset, std::span<const uint8_t>{new_data_mem, new_data_size});
    }

    template <typename T>
        requires(!std::convertible_to<T, std::span<const uint8_t>>)
    auto Update(uint64_t offset, const T &new_data) -> void {
        const auto new_data_mem = reinterpret_cast<const uint8_t *>(&new_data);
        const auto new_data_size = sizeof(T);

        Update(offset, std::span<const uint8_t>{new_data_mem, new_data_size});
    }

    template <std::invocable<const BufferHandle &> F> auto View(BufferType view_type, F consumer) const -> void {
        const auto view_type_gl = static_cast<GLenum>(view_type);
        executor_.RunOnContext([&](const auto &) {
            GL_CHECK(glBindBuffer(view_type_gl, handle_));
            consumer(handle_);
            GL_CHECK(glBindBuffer(view_type_gl, 0));
        });
    }

private:
    GLContext::Executor executor_;
    BufferHandle handle_;

    BufferType initial_type_;
    GLenum type_gl_;

    BufferUsage usage_;
    uint64_t size_;

    void *cpu_mapped_;
};

template <typename T> class UniformBuffer {
private:
    static auto AllocateBackingBuffer(GLContext::Executor executor, size_t size) {
        const Buffer::Description description = {
            .type = BufferType::eUniformBuffer,
            .usage = BufferUsage::eStaticDraw,
            .size = size,
            .init_data = std::nullopt,
        };

        return gl::Buffer{std::move(executor), description};
    }

public:
    UniformBuffer(GLContext::Executor &&executor, T data)
        : executor_{executor}, backing_buffer_{AllocateBackingBuffer(executor_, sizeof(T))}, data_{data} {}

    UniformBuffer(const UniformBuffer &) = delete;
    auto operator=(const UniformBuffer &) = delete;

    UniformBuffer(UniformBuffer &&) = default;
    auto operator=(UniformBuffer &&) -> UniformBuffer & = default;

    auto GetExecutor() const -> const GLContext::Executor & { return executor_; }
    auto BackingBuffer() const -> const Buffer & { return backing_buffer_; }

    auto GetData() const -> const T & { return data_; }
    auto GetData() -> T & { return data_; }

    auto ExecuteUpload() -> void {
        const auto data_ptr = reinterpret_cast<const uint8_t *>(&data_);
        const auto data_size = sizeof(T);

        backing_buffer_.Update(0, std::span<const uint8_t>{data_ptr, data_size});
    }

    auto BindBuffer(uint32_t bind_index) const -> void {
        executor_.RunOnContext([&](const auto &) {
            GL_CHECK(glBindBufferBase(GL_UNIFORM_BUFFER, bind_index, backing_buffer_.GetHandle()));
        });
    }

private:
    GLContext::Executor executor_;

    Buffer backing_buffer_;
    T data_;
};

} // namespace gl
