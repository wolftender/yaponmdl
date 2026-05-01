#include "graphics/buffer.hpp"

namespace gl {

Buffer::Buffer(GLContext::Executor &&executor, const Description &description)
    : executor_{executor}, handle_{gl::factories::MakeBuffer(executor_)}, initial_type_{description.type},
      type_gl_{static_cast<GLenum>(initial_type_)}, usage_{description.usage}, size_{description.size},
      cpu_mapped_{nullptr} {
    assert(!description.init_data.has_value() || description.init_data->size() == description.size);

    executor_.RunOnContext([&](const auto &) {
        const void *init_data_ptr = description.init_data.has_value() ? description.init_data->data() : nullptr;
        const auto buffer_size = description.size;

        GL_CHECK(glBindBuffer(type_gl_, handle_));
        GL_CHECK(glBufferData(type_gl_, buffer_size, init_data_ptr, static_cast<GLenum>(usage_)));
        GL_CHECK(glBindBuffer(type_gl_, 0));
    });
}

auto Buffer::Orphan(uint64_t offset, std::optional<std::span<const uint8_t>> new_data) -> void {
    executor_.RunOnContext([&](const auto &) {
        GL_CHECK(glBindBuffer(type_gl_, handle_));

        if (new_data.has_value()) {
            size_ = new_data->size();
            GL_CHECK(glBufferData(type_gl_, size_, nullptr, static_cast<GLenum>(usage_)));
            GL_CHECK(glBufferSubData(type_gl_, offset, new_data->size_bytes(), new_data->data()));
        } else {
            GL_CHECK(glBufferData(type_gl_, size_, nullptr, static_cast<GLenum>(usage_)));
        }

        GL_CHECK(glBindBuffer(type_gl_, 0));
    });
}

auto Buffer::Update(uint64_t offset, std::span<const uint8_t> new_data) -> void {
    assert(offset + new_data.size() <= size_);
    executor_.RunOnContext([&](const auto &) {
        GL_CHECK(glBindBuffer(type_gl_, handle_));
        GL_CHECK(glBufferSubData(type_gl_, offset, new_data.size_bytes(), new_data.data()));
        GL_CHECK(glBindBuffer(type_gl_, 0));
    });
}

} // namespace gl
