#include "graphics/gl.hpp"

namespace gl {

thread_local std::shared_ptr<GLContext::Controller> GLContext::Executor::s_controller = nullptr;

namespace factories {

auto MakeShader(GLContext::Executor executor, const GLenum type) -> ShaderHandle {
    GLuint handle = 0;
    executor.RunOnContext([&](const auto &) { handle = GL_CHECK(glCreateShader(type)); });

    return {handle, deleters::Shader{executor}};
}

auto MakeProgram(GLContext::Executor executor) -> ShaderProgramHandle {
    GLuint handle = 0;
    executor.RunOnContext([&](const auto &) { handle = GL_CHECK(glCreateProgram()); });

    return {handle, deleters::ShaderProgram{executor}};
}

auto MakeBuffer(GLContext::Executor executor) -> BufferHandle {
    GLuint handle = 0;
    glGenBuffers(1, &handle);
    return {handle, deleters::Buffer{executor}};
}

auto MakeArray(GLContext::Executor executor) -> VertexArrayHandle {
    GLuint handle = 0;
    executor.RunOnContext([&](const auto &) { GL_CHECK(glGenVertexArrays(1, &handle)); });

    return {handle, deleters::VertexArray{executor}};
}

auto MakeTexture(GLContext::Executor executor) -> TextureHandle {
    GLuint handle = 0;
    executor.RunOnContext([&](const auto &) { GL_CHECK(glGenTextures(1, &handle)); });

    return {handle, deleters::Texture{executor}};
}

auto MakeFramebuffer(GLContext::Executor executor) -> FramebufferHandle {
    GLuint handle = 0;
    executor.RunOnContext([&](const auto &) { GL_CHECK(glGenFramebuffers(1, &handle)); });

    return {handle, deleters::Framebuffer{executor}};
}

auto MakeRenderbuffer(GLContext::Executor executor) -> RenderbufferHandle {
    GLuint handle = 0;
    executor.RunOnContext([&](const auto &) { GL_CHECK(glGenRenderbuffers(1, &handle)); });

    return {handle, deleters::Renderbuffer{executor}};
}

} // namespace factories

} // namespace gl
