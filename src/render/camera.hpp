#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include "render/device.hpp"

namespace render {

class PerspectiveCamera : public hal::ICamera {
public:
    PerspectiveCamera()
        : aspect_{1.0f}, fov_{glm::pi<float>() * 0.5f}, near_{0.5f}, far_{200.0f}, dirty_bit_proj_{true},
          projection_{1.0f}, projection_inv_{1.0f} {}

    auto GetAspect() const -> float { return aspect_; }
    auto GetFov() const -> float { return fov_; }
    auto GetNear() const -> float { return near_; }
    auto GetFar() const -> float { return far_; }

    auto SetAspect(float aspect) -> void;
    auto SetFov(float fov) -> void;
    auto SetNear(float near) -> void;
    auto SetFar(float far) -> void;

    auto GetProjection() const -> const glm::fmat4x4 & override;
    auto GetView() const -> const glm::fmat4x4 & override;

    auto GetProjectionInv() const -> const glm::fmat4x4 &;
    auto GetViewInv() const -> const glm::fmat4x4 &;

private:
    inline auto CalculateProjection() const -> void;

    float aspect_, fov_, near_, far_;

    // cache
    mutable bool dirty_bit_proj_;
    mutable glm::fmat4x4 projection_, projection_inv_;
};

class LookAtCamera : public PerspectiveCamera {
public:
    LookAtCamera()
        : position_{0.0f, 0.0f, 1.0f}, target_{0.0f, 0.0f, 0.0f}, dirty_bit_view_{true}, view_{1.0f}, view_inv_{1.0f} {}

    auto GetPosition() const -> const glm::fvec3 & { return position_; }
    auto GetTarget() const -> const glm::fvec3 & { return target_; }

    auto SetPosition(const glm::fvec3 &position) -> void;
    auto SetTarget(const glm::fvec3 &target) -> void;

    auto GetView() const -> const glm::fmat4x4 & override;
    auto GetViewInv() const -> const glm::fmat4x4 &;

private:
    inline auto CalculateView() const -> void;

    glm::fvec3 position_;
    glm::fvec3 target_;

    mutable bool dirty_bit_view_;
    mutable glm::fmat4x4 view_, view_inv_;
};

class AzimuthCamera : public PerspectiveCamera {
public:
    AzimuthCamera()
        : center_{0.0f, 0.0f, 0.0f}, distance_{1.0f}, azimuth_{0.0f}, elevation_{0.0f}, dirty_bit_view_{true},
          view_{1.0f}, view_inv_{1.0f} {}

    auto GetCenter() const -> const glm::fvec3 & { return center_; }
    auto GetDistance() const -> float { return distance_; }
    auto GetAzimuth() const -> float { return azimuth_; }
    auto GetElevation() const -> float { return elevation_; }

    auto SetCenter(const glm::fvec3 &center) -> void;
    auto SetDistance(float distance) -> void;
    auto SetAzimuth(float azimuth) -> void;
    auto SetElevation(float elevation) -> void;

    auto GetView() const -> const glm::fmat4x4 & override;
    auto GetViewInv() const -> const glm::fmat4x4 &;

private:
    inline auto CalculateView() const -> void;

    glm::fvec3 center_;
    float distance_, azimuth_, elevation_;

    mutable bool dirty_bit_view_;
    mutable glm::fmat4x4 view_, view_inv_;
};

class OrthoCamera : public hal::ICamera {
public:
    OrthoCamera()
        : center_{0.0f, 0.0f, 0.0f}, size_{2.0f, 2.0f}, near_{-1.0f}, far_{1.0f}, dirty_bit_view_{true},
          dirty_bit_proj_{true} {}

    auto GetCenter() const -> const glm::fvec3 & { return center_; }
    auto GetSize() const -> const glm::fvec2 & { return size_; }
    auto GetNear() const -> float { return near_; }
    auto GetFar() const -> float { return far_; }

    auto SetCenter(const glm::fvec3 &center) -> void;
    auto SetSize(const glm::fvec2 &size) -> void;
    auto SetNear(float near) -> void;
    auto SetFar(float far) -> void;

    auto GetProjection() const -> const glm::fmat4x4 & override;
    auto GetView() const -> const glm::fmat4x4 & override;

    auto GetProjectionInv() const -> const glm::fmat4x4 &;
    auto GetViewInv() const -> const glm::fmat4x4 &;

private:
    inline auto CalculateView() const -> void;
    inline auto CalculateProjection() const -> void;

    glm::fvec3 center_;
    glm::fvec2 size_;
    float near_, far_;

    mutable bool dirty_bit_view_;
    mutable bool dirty_bit_proj_;

    mutable glm::fmat4x4 view_, view_inv_;
    mutable glm::fmat4x4 projection_, projection_inv_;
};

} // namespace render