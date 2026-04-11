#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include "render/device.hpp"

namespace render {

class PerspectiveCamera final : public hal::ICamera {
public:
    PerspectiveCamera()
        : position_{0.0f, 0.0f, 1.0f}, target_{0.0f, 0.0f, 0.0f}, aspect_{1.0f}, fov_{glm::pi<float>() * 0.5f},
          near_{0.5f}, far_{200.0f}, dirty_bit_proj_{true}, dirty_bit_view_{true}, projection_{1.0f},
          projection_inv_{1.0f}, view_{1.0f}, view_inv_{1.0f} {}

    auto GetPosition() const -> const glm::fvec3 & { return position_; }
    auto GetTarget() const -> const glm::fvec3 & { return target_; }
    auto GetAspect() const -> float { return aspect_; }
    auto GetFov() const -> float { return fov_; }
    auto GetNear() const -> float { return near_; }
    auto GetFar() const -> float { return far_; }

    auto SetPosition(const glm::fvec3 &position) -> void;
    auto SetTarget(const glm::fvec3 &target) -> void;
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
    inline auto CalculateView() const -> void;

    glm::fvec3 position_;
    glm::fvec3 target_;

    float aspect_, fov_, near_, far_;

    // cache
    mutable bool dirty_bit_proj_;
    mutable bool dirty_bit_view_;
    mutable glm::fmat4x4 projection_, projection_inv_;
    mutable glm::fmat4x4 view_, view_inv_;
};

} // namespace render