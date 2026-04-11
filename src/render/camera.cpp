#include "render/camera.hpp"

namespace render {

auto PerspectiveCamera::SetPosition(const glm::fvec3 &position) -> void {
    position_ = position;
    dirty_bit_view_ = true;
}

auto PerspectiveCamera::SetTarget(const glm::fvec3 &target) -> void {
    target_ = target;
    dirty_bit_view_ = true;
}

auto PerspectiveCamera::SetAspect(float aspect) -> void {
    aspect_ = aspect;
    dirty_bit_proj_ = true;
}

auto PerspectiveCamera::SetFov(float fov) -> void {
    fov_ = fov;
    dirty_bit_proj_ = true;
}

auto PerspectiveCamera::SetNear(float near) -> void {
    near_ = near;
    dirty_bit_proj_ = true;
}

auto PerspectiveCamera::SetFar(float far) -> void {
    far_ = far;
    dirty_bit_proj_ = true;
}

auto PerspectiveCamera::GetProjection() const -> const glm::fmat4x4 & {
    CalculateProjection();
    return projection_;
}

auto PerspectiveCamera::GetView() const -> const glm::fmat4x4 & {
    CalculateView();
    return view_;
}

auto PerspectiveCamera::GetProjectionInv() const -> const glm::fmat4x4 & {
    CalculateProjection();
    return projection_inv_;
}

auto PerspectiveCamera::GetViewInv() const -> const glm::fmat4x4 & {
    CalculateView();
    return view_inv_;
}

inline auto PerspectiveCamera::CalculateProjection() const -> void {
    if (dirty_bit_proj_) {
        projection_ = glm::perspective(fov_, aspect_, near_, far_);
        projection_inv_ = glm::inverse(projection_);
        dirty_bit_proj_ = false;
    }
}

inline auto PerspectiveCamera::CalculateView() const -> void {
    if (dirty_bit_view_) {
        view_ = glm::lookAt(position_, target_, glm::fvec3{0.0f, 1.0f, 0.0f});
        view_inv_ = glm::inverse(view_);
        dirty_bit_view_ = false;
    }
}

} // namespace render