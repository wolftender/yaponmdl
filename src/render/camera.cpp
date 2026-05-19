#include "render/camera.hpp"

namespace render {

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

auto PerspectiveCamera::GetView() const -> const glm::fmat4x4 & {
    static const glm::fmat4x4 kIdentity{1.0f};
    return kIdentity;
}

auto PerspectiveCamera::GetViewInv() const -> const glm::fmat4x4 & {
    static const glm::fmat4x4 kIdentity{1.0f};
    return kIdentity;
}

auto PerspectiveCamera::GetProjection() const -> const glm::fmat4x4 & {
    CalculateProjection();
    return projection_;
}

auto PerspectiveCamera::GetProjectionInv() const -> const glm::fmat4x4 & {
    CalculateProjection();
    return projection_inv_;
}

inline auto PerspectiveCamera::CalculateProjection() const -> void {
    if (dirty_bit_proj_) {
        projection_ = glm::perspective(fov_, aspect_, near_, far_);
        projection_inv_ = glm::inverse(projection_);
        dirty_bit_proj_ = false;
    }
}

auto LookAtCamera::SetPosition(const glm::fvec3 &position) -> void {
    position_ = position;
    dirty_bit_view_ = true;
}

auto LookAtCamera::SetTarget(const glm::fvec3 &target) -> void {
    target_ = target;
    dirty_bit_view_ = true;
}

auto LookAtCamera::GetView() const -> const glm::fmat4x4 & {
    CalculateView();
    return view_;
}

auto LookAtCamera::GetViewInv() const -> const glm::fmat4x4 & {
    CalculateView();
    return view_inv_;
}

inline auto LookAtCamera::CalculateView() const -> void {
    if (dirty_bit_view_) {
        view_ = glm::lookAt(position_, target_, glm::fvec3{0.0f, 1.0f, 0.0f});
        view_inv_ = glm::inverse(view_);
        dirty_bit_view_ = false;
    }
}

auto AzimuthCamera::SetCenter(const glm::fvec3 &center) -> void {
    center_ = center;
    dirty_bit_view_ = true;
}

auto AzimuthCamera::SetDistance(float distance) -> void {
    distance_ = distance;
    dirty_bit_view_ = true;
}

auto AzimuthCamera::SetAzimuth(float azimuth) -> void {
    azimuth_ = std::fmodf(azimuth, 2.0f * glm::pi<float>());
    dirty_bit_view_ = true;
}

auto AzimuthCamera::SetElevation(float elevation) -> void {
    elevation_ = std::fmodf(elevation, 2.0f * glm::pi<float>());
    dirty_bit_view_ = true;
}

auto AzimuthCamera::GetView() const -> const glm::fmat4x4 & {
    CalculateView();
    return view_;
}

auto AzimuthCamera::GetViewInv() const -> const glm::fmat4x4 & {
    CalculateView();
    return view_inv_;
}

inline auto AzimuthCamera::CalculateView() const -> void {
    if (dirty_bit_view_) {
        view_ = glm::fmat4x4{1.0f};
        view_ = glm::translate(view_, glm::fvec3{0.0f, 0.0f, -distance_});
        view_ = glm::rotate(view_, -elevation_, glm::fvec3{1.0f, 0.0f, 0.0f});
        view_ = glm::rotate(view_, -azimuth_, glm::fvec3{0.0f, 1.0f, 0.0f});
        view_ = glm::translate(view_, -center_);

        view_inv_ = glm::fmat4x4{1.0f};
        view_inv_ = glm::translate(view_inv_, center_);
        view_inv_ = glm::rotate(view_inv_, azimuth_, glm::fvec3{0.0f, 1.0f, 0.0f});
        view_inv_ = glm::rotate(view_inv_, elevation_, glm::fvec3{1.0f, 0.0f, 0.0f});
        view_inv_ = glm::translate(view_inv_, glm::fvec3{0.0f, 0.0f, distance_});

        dirty_bit_view_ = false;
    }
}

auto OrthoCamera::SetCenter(const glm::fvec3 &center) -> void {
    center_ = center;
    dirty_bit_view_ = true;
}

auto OrthoCamera::SetSize(const glm::fvec2 &size) -> void {
    size_ = size;
    dirty_bit_proj_ = true;
}

auto OrthoCamera::SetNear(float near) -> void {
    near_ = near;
    dirty_bit_proj_ = true;
}

auto OrthoCamera::SetFar(float far) -> void {
    far_ = far;
    dirty_bit_proj_ = true;
}

auto OrthoCamera::GetProjection() const -> const glm::fmat4x4 & {
    CalculateProjection();
    return projection_;
}

auto OrthoCamera::GetView() const -> const glm::fmat4x4 & {
    CalculateView();
    return view_;
}

auto OrthoCamera::GetProjectionInv() const -> const glm::fmat4x4 & {
    CalculateProjection();
    return projection_inv_;
}

auto OrthoCamera::GetViewInv() const -> const glm::fmat4x4 & {
    CalculateView();
    return view_inv_;
}

inline auto OrthoCamera::CalculateView() const -> void {
    if (dirty_bit_view_) {
        view_ = glm::translate(glm::fmat4x4{1.0f}, -center_);
        view_inv_ = glm::translate(glm::fmat4x4{1.0f}, center_);
        dirty_bit_view_ = false;
    }
}

inline auto OrthoCamera::CalculateProjection() const -> void {
    if (dirty_bit_proj_) {
        const auto half_size = size_ * 0.5f;
        projection_ = glm::ortho(-half_size.x, half_size.x, -half_size.y, half_size.y, near_, far_);
        projection_inv_ = glm::inverse(projection_);

        dirty_bit_proj_ = false;
    }
}

} // namespace render