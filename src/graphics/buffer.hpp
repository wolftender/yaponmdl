#pragma once
#include "graphics/gl.hpp"

namespace gl {

template <typename T> class UniformBuffer final {
public:
    UniformBuffer(GLContext::Executor &&executor, const T &data, uint32_t base)
        : executor_{executor}, handle_{factories::MakeBuffer(executor_)}, buffer_base_{base}, data_{data} {
        executor_.RunOnContext([&](const auto &) {
            GL_CHECK(glBindBuffer(GL_UNIFORM_BUFFER, handle_));
            GL_CHECK(glBufferData(GL_UNIFORM_BUFFER, sizeof(T), &data, GL_STATIC_DRAW));
            GL_CHECK(glBindBufferBase(GL_UNIFORM_BUFFER, buffer_base_, handle_));
            GL_CHECK(glBindBuffer(GL_UNIFORM_BUFFER, 0));
        });
    }

    UniformBuffer(const UniformBuffer &) = delete;
    auto operator=(const UniformBuffer &) -> UniformBuffer & = delete;

    UniformBuffer(UniformBuffer &&) = default;
    auto operator=(UniformBuffer &&) -> UniformBuffer & = default;

    auto GetData() const -> const T & { return data_; }
    auto GetData() -> T & { return data_; }
    auto GetBase() const -> uint32_t { return buffer_base_; }

    auto SetData(const T &data) -> void { data_ = data; }

    auto Update() {
        executor_.RunOnContext([&](const auto &) {
            GL_CHECK(glBindBuffer(GL_UNIFORM_BUFFER, handle_));
            GL_CHECK(glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(T), &data_));
            GL_CHECK(glBindBuffer(GL_UNIFORM_BUFFER, 0));
        });
    }

    auto GetExecutor() const -> const GLContext::Executor & { return executor_; }

private:
    GLContext::Executor executor_;
    BufferHandle handle_;
    uint32_t buffer_base_;
    T data_;

    friend class ShaderProgram;
};

} // namespace gl
