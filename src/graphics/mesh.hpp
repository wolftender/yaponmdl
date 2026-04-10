#pragma once
#include "common/common.hpp"
#include "graphics/gl.hpp"

namespace gl {

template <VertexType V> class Mesh final {
public:
    template <TypedContiguousRange<V> R>
    Mesh(GLContext::Executor executor, const R &vertices)
        : executor_{executor}, vertex_array_{factories::MakeArray(executor)},
          vertex_buffer_{factories::MakeBuffer(executor)}, index_buffer_{executor},
          num_elements_{static_cast<uint32_t>(std::ranges::size(vertices))} {
        executor_.RunOnContext([&](const auto &) {
            GL_CHECK(glBindVertexArray(vertex_array_));
            InitVertexBuffer(vertices);
            GL_CHECK(glBindVertexArray(0));
        });
    }

    template <TypedContiguousRange<V> R, TypedContiguousRange<uint32_t> I>
    Mesh(GLContext::Executor executor, const R &vertices, const I &indices)
        : executor_{executor}, vertex_array_{factories::MakeArray(executor)},
          vertex_buffer_{factories::MakeBuffer(executor)}, index_buffer_{factories::MakeBuffer(executor)},
          num_elements_{static_cast<uint32_t>(std::ranges::size(indices))} {
        executor_.RunOnContext([&](const auto &) {
            GL_CHECK(glBindVertexArray(vertex_array_));
            InitVertexBuffer(vertices);
            InitIndexBuffer(indices);
            GL_CHECK(glBindVertexArray(0));
        });
    }

    ~Mesh() = default;

    Mesh(const Mesh &) = delete;
    auto operator=(const Mesh &) -> Mesh & = delete;

    Mesh(Mesh &&) = default;
    auto operator=(Mesh &&) -> Mesh & = default;

    inline auto Draw(uint64_t instances = 1) const {
        Draw(instances, [&](const auto &_) {});
    }

    template <std::invocable<uint64_t> F> inline auto Draw(uint64_t instances, const F &&f) const {
        executor_.RunOnContext([&](const auto &) {
            GL_CHECK(glBindVertexArray(vertex_array_));

            if (index_buffer_) {
                for (uint64_t instance = 0; instance < instances; ++instance) {
                    f(instance);
                    GL_CHECK(glDrawElements(GL_TRIANGLES, num_elements_, GL_UNSIGNED_INT, nullptr));
                }
            } else {
                for (uint64_t instance = 0; instance < instances; ++instance) {
                    f(instance);
                    GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, num_elements_));
                }
            }

            GL_CHECK(glBindVertexArray(0));
        });
    }

    auto GetExecutor() const -> const GLContext::Executor & { return executor_; }

private:
    template <TypedContiguousRange<V> R> auto InitVertexBuffer(const R &vertices) {
        GL_IMPLEMENTATION_INTERNAL;

        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_));
        GL_CHECK(glBufferData(
            GL_ARRAY_BUFFER, sizeof(V) * std::ranges::size(vertices), std::ranges::data(vertices), GL_STATIC_DRAW));

        for (const LayoutElement &layout_element : V::GetLayout()) {
            const auto layout_offset = static_cast<uintptr_t>(layout_element.offset);

            GL_CHECK(glVertexAttribPointer(
                static_cast<GLuint>(layout_element.index), static_cast<GLint>(layout_element.size),
                static_cast<GLenum>(layout_element.type), GL_FALSE, static_cast<GLsizei>(layout_element.stride),
                reinterpret_cast<void *>(layout_offset)));
            GL_CHECK(glEnableVertexAttribArray(layout_element.index));
        }
    }

    template <TypedRandomAccessRange<uint32_t> I> auto InitIndexBuffer(const I &indices) {
        GL_IMPLEMENTATION_INTERNAL;

        if (index_buffer_) {
            GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_));
            GL_CHECK(glBufferData(
                GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * std::ranges::size(indices), std::ranges::data(indices),
                GL_STATIC_DRAW));
        }
    }

    GLContext::Executor executor_;
    VertexArrayHandle vertex_array_;
    BufferHandle vertex_buffer_;
    BufferHandle index_buffer_;
    uint32_t num_elements_;
};

} // namespace gl
