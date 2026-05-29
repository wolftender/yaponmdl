#include "graphics/shader.hpp"

namespace gl {

ShaderProgram::ShaderProgram(
    GLContext::Executor executor, const std::string_view &vs_source, const std::string_view &fs_source,
    std::optional<std::span<BlockLayoutDesc>> ubo_bindings)
    : executor_{executor}, handle_{factories::MakeProgram(executor_)} {

    executor_.RunOnContext([&](const auto &) {
        auto vs = CompileShader(executor_, GL_VERTEX_SHADER, vs_source);
        auto fs = CompileShader(executor_, GL_FRAGMENT_SHADER, fs_source);

        GL_CHECK(glAttachShader(handle_, vs));
        GL_CHECK(glAttachShader(handle_, fs));
        GL_CHECK(glLinkProgram(handle_));

        GLint status = GL_FALSE;
        GL_CHECK(glGetProgramiv(handle_, GL_LINK_STATUS, &status));
        if (GL_TRUE != status) {
            constexpr std::size_t kMaxLogLength = 4096UL;

            GLsizei len = 0;
            std::array<char, kMaxLogLength> buffer;

            std::fill(buffer.begin(), buffer.end(), 0);
            GL_CHECK(glGetProgramInfoLog(handle_, kMaxLogLength, &len, buffer.data()));

            throw ShaderLinkerError{std::string(buffer.data(), len)};
        }

        MapUniforms();
        MapUniformBlocks(ubo_bindings);
    });
}

auto ShaderProgram::MapUniforms() -> void {
    GL_IMPLEMENTATION_INTERNAL;

    GLint num_uniforms = 0;
    GL_CHECK(glGetProgramiv(handle_, GL_ACTIVE_UNIFORMS, &num_uniforms));

    constexpr std::size_t kMaxUniformNameLen = 512UL;
    std::array<char, kMaxUniformNameLen> name_buf;

    for (GLuint uniform = 0; uniform < static_cast<GLuint>(num_uniforms); ++uniform) {
        std::fill(name_buf.begin(), name_buf.end(), 0);

        GLint size = 0;
        GLenum type = 0;
        GL_CHECK(glGetActiveUniform(handle_, uniform, kMaxUniformNameLen, nullptr, &size, &type, name_buf.data()));

        auto location = GL_CHECK(glGetUniformLocation(handle_, name_buf.data()));
        std::string name{name_buf.data()};

        uniforms_.insert({name, {name, location, size, type}});
    }
}

auto ShaderProgram::MapUniformBlocks(std::optional<std::span<BlockLayoutDesc>> ubo_bindings) -> void {
    GL_IMPLEMENTATION_INTERNAL;

    GLint num_uniform_blocks = 0;
    GL_CHECK(glGetProgramiv(handle_, GL_ACTIVE_UNIFORM_BLOCKS, &num_uniform_blocks));

    constexpr std::size_t kMaxBlockNameLen = 512UL;
    std::array<char, kMaxBlockNameLen> name_buf;
    std::fill(name_buf.begin(), name_buf.end(), 0);

    for (GLuint block = 0; block < static_cast<GLuint>(num_uniform_blocks); ++block) {
        GLint data_size = 0;

        GL_CHECK(glGetActiveUniformBlockiv(handle_, block, GL_UNIFORM_BLOCK_DATA_SIZE, &data_size));
        GL_CHECK(glGetActiveUniformBlockName(handle_, block, kMaxBlockNameLen, nullptr, name_buf.data()));
        auto index = GL_CHECK(glGetUniformBlockIndex(handle_, name_buf.data()));
        std::string name{name_buf.data()};

        uint32_t ubo_index = 0;
        if (ubo_bindings.has_value()) {
            const auto binding = std::find_if(
                ubo_bindings->begin(), ubo_bindings->end(), [&](const auto &binding) { return binding.first == name; });
            if (binding != ubo_bindings->end()) {
                ubo_index = binding->second;
            }
        }

        uniform_blocks_.insert({name, {name, data_size, index, ubo_index}});
    }
}

auto ShaderProgram::CompileShader(GLContext::Executor executor, const GLenum type, const std::string_view &source)
    -> ShaderHandle {
    GL_IMPLEMENTATION_INTERNAL;

    auto handle = factories::MakeShader(executor, type);

    const char *ptr_source = source.data();
    const auto source_len = static_cast<int>(source.size());

    GL_CHECK(glShaderSource(handle, 1, &ptr_source, &source_len));
    GL_CHECK(glCompileShader(handle));

    GLint status = GL_FALSE;
    GL_CHECK(glGetShaderiv(handle, GL_COMPILE_STATUS, &status));
    if (GL_TRUE != status) {
        constexpr auto kMaxLogLength = 4096UL;

        GLsizei log_len = 0;
        std::array<char, kMaxLogLength> buffer;

        std::fill(buffer.begin(), buffer.end(), 0);
        GL_CHECK(glGetShaderInfoLog(handle, kMaxLogLength, &log_len, buffer.data()));

        throw ShaderCompileError{std::string(buffer.data(), log_len)};
    }

    return handle;
}

} // namespace gl
