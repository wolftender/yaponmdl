#include <algorithm>

#include "render/drawlist.hpp"

namespace render {

auto Drawlist::Motion::Frame::GetNodeOffset(NodeId node) const -> const glm::fmat4x4 & {
    static const glm::fmat4x4 &identity = glm::fmat4x4{1.0f};
    if (node.index() >= node_offsets_.size()) {
        return identity;
    }

    return node_offsets_[node.index()];
}

auto Drawlist::Motion::Frame::SetNodeOffset(NodeId node, const glm::fmat4x4 &offset) -> void {
    if (node_offsets_.size() == node.index()) {
        node_offsets_.push_back(offset);
        return;
    }

    if (node_offsets_.size() <= node.index()) {
        node_offsets_.resize(node.index() + 1, glm::fmat4x4{1.0f});
    }

    node_offsets_[node.index()] = offset;
}

auto Drawlist::Motion::AppendFrame(Frame &&frame) -> void {
    frames_.emplace_back(std::move(frame));
    duration_ = std::max(frame.GetTime(), duration_);
}

auto Drawlist::Motion::Sort() -> void {
    std::sort(frames_.begin(), frames_.end(), [&](const auto &frame_l, const auto &frame_r) -> bool {
        return frame_l.GetTime() < frame_r.GetTime();
    });
}

auto Drawlist::Controller::GetDuration() const -> float {
    const auto *motion = model_->GetMotion(motion_);
    if (motion) {
        return motion->GetDuration();
    }

    return 0.0f;
}

auto Drawlist::Controller::GetNumFrames() const -> uint32_t {
    const auto *motion = model_->GetMotion(motion_);
    if (motion) {
        return motion->GetFrames().size();
    }

    return 0;
}

auto Drawlist::Controller::SetMotion(MotionId motion) -> void {
    motion_ = motion;
    time_ = 0.0f;

    const auto motion_ptr = model_->GetMotion(motion);
    if (motion_ptr) {
        InitMotionData(*motion_ptr);
    }
}

auto Drawlist::Controller::Seek(float time) -> void {
    auto motion = model_->GetMotion(motion_);
    if (motion) {
        time_ = loop_ ? std::fmod(std::max(0.0f, time), motion->GetDuration()) : std::max(0.0f, time);
        ResetMotion(*motion);
        UpdateMotion(*motion);
    }
}

auto Drawlist::Controller::Integrate(float delta_time) -> void {
    time_ = time_ + delta_time;

    auto motion = model_->GetMotion(motion_);
    if (motion) {
        if (loop_ && time_ > motion->GetDuration()) {
            time_ = std::fmod(time_, motion->GetDuration());
            ResetMotion(*motion);
        }

        UpdateMotion(*motion);
    }
}

auto Drawlist::Controller::Render(const glm::fmat4x4 &world_matrix) const -> void {
    const auto motion = model_->GetMotion(motion_);
    if (!motion) {
        return;
    }

    const auto &keyframe = motion->GetFrames()[current_frame_];
    for (const auto &draw_command : keyframe.GetCommands()) {
        std::visit(
            overload{
                [&](const Motion::DrawCommand &command) {
            const auto &vert_buffer = model_->vertex_buffers_[command.mesh.index()];

            hal::StaticDrawDescription draw_desc = {
                .mesh = vert_buffer.GetHandle(),
                .world_matrix = world_matrix * command.parameters.world_matix,
                .color = command.parameters.color,
                .uv_offset = command.parameters.uv_offset,
                .uv_scale = command.parameters.uv_scale,
                .alpha = glm::fvec1{1.0f},
                .diffuse_map = std::nullopt,
                .normal_map = std::nullopt,
            };

            if (command.parameters.diffuse_map.has_value()) {
                const auto &texture = model_->textures_[command.parameters.diffuse_map.value().index()];
                draw_desc.diffuse_map = texture.GetHandle();
            }

            device_->SubmitStaticDraw(std::move(draw_desc));
        },
                [&](const Motion::SkinnedDrawCommand &skinned_command) {
            const auto &vert_buffer = model_->skinned_vertex_buffers_[skinned_command.mesh.index()];
            const auto &skin = model_->skins_[skinned_command.skin.index()];

            hal::SkinnedDrawDescription draw_desc = {
                .mesh = vert_buffer.GetHandle(),
                .skinning_buffer = skin.GetHandle(),
                .world_matrix = world_matrix * skinned_command.parameters.world_matix,
                .color = skinned_command.parameters.color,
                .uv_offset = skinned_command.parameters.uv_offset,
                .uv_scale = skinned_command.parameters.uv_scale,
                .alpha = glm::fvec1{1.0f},
                .diffuse_map = std::nullopt,
                .normal_map = std::nullopt,
            };

            if (skinned_command.parameters.diffuse_map.has_value()) {
                const auto &texture = model_->textures_[skinned_command.parameters.diffuse_map.value().index()];
                draw_desc.diffuse_map = texture.GetHandle();
            }

            device_->SubmitSkinnedDraw(std::move(draw_desc));
        }},
            draw_command);
    }
}

auto Drawlist::Controller::InitMotionData(const Motion &motion) -> void {
    const auto keyframes = motion.GetFrames();
    const auto num_keyframes = keyframes.size();
    if (num_keyframes <= 1) {
        current_frame_ = 0;
        current_frame_time_ = 0.0f;
        next_frame_time_ = 9999.0f;
    } else {
        current_frame_ = 0;
        current_frame_time_ = keyframes[0].GetTime();
        next_frame_time_ = keyframes[1].GetTime();
    }
}

auto Drawlist::Controller::UpdateMotion(const Motion &motion) -> void {
    const auto keyframes = motion.GetFrames();
    if (time_ >= next_frame_time_) {
        const auto num_keyframes = static_cast<uint32_t>(keyframes.size());
        while (current_frame_ < num_keyframes && keyframes[current_frame_].GetTime() < time_) {
            current_frame_++;
        }

        current_frame_time_ = keyframes[current_frame_].GetTime();
        next_frame_time_ = keyframes[std::min(current_frame_ + 1, num_keyframes - 1)].GetTime();
    }
}

auto Drawlist::Controller::ResetMotion(const Motion &motion) -> void { InitMotionData(motion); }

auto Drawlist::CreateController(MotionId motion) const -> Controller { return Controller{this, motion}; }

auto Drawlist::GetVertexBuffer(VertexBufferId handle) -> VertexBuffer * {
    if (handle.index() >= vertex_buffers_.size()) {
        return nullptr;
    }

    return &vertex_buffers_[handle.index()];
}

auto Drawlist::GetSkinnedVertexBuffer(SkinnedVertexBufferId handle) -> SkinnedVertexBuffer * {
    if (handle.index() >= skinned_vertex_buffers_.size()) {
        return nullptr;
    }

    return &skinned_vertex_buffers_[handle.index()];
}

auto Drawlist::GetNamedNode(NodeId handle) -> Node * {
    if (handle.index() >= named_nodes_.size()) {
        return nullptr;
    }

    return &named_nodes_[handle.index()];
}

auto Drawlist::GetTexture(TextureId handle) -> Texture * {
    if (handle.index() >= textures_.size()) {
        return nullptr;
    }

    return &textures_[handle.index()];
}

auto Drawlist::GetSkin(SkinId handle) -> Skin * {
    if (handle.index() >= skins_.size()) {
        return nullptr;
    }

    return &skins_[handle.index()];
}

auto Drawlist::GetMotion(MotionId handle) -> Motion * {
    if (handle.index() >= motions_.size()) {
        return nullptr;
    }

    return &motions_[handle.index()];
}

auto Drawlist::GetVertexBuffer(VertexBufferId handle) const -> const VertexBuffer * {
    if (handle.index() >= vertex_buffers_.size()) {
        return nullptr;
    }

    return &vertex_buffers_[handle.index()];
}

auto Drawlist::GetSkinnedVertexBuffer(SkinnedVertexBufferId handle) const -> const SkinnedVertexBuffer * {
    if (handle.index() >= skinned_vertex_buffers_.size()) {
        return nullptr;
    }

    return &skinned_vertex_buffers_[handle.index()];
}

auto Drawlist::GetNamedNode(NodeId handle) const -> const Node * {
    if (handle.index() >= named_nodes_.size()) {
        return nullptr;
    }

    return &named_nodes_[handle.index()];
}

auto Drawlist::GetTexture(TextureId handle) const -> const Texture * {
    if (handle.index() >= textures_.size()) {
        return nullptr;
    }

    return &textures_[handle.index()];
}

auto Drawlist::GetSkin(SkinId handle) const -> const Skin * {
    if (handle.index() >= skins_.size()) {
        return nullptr;
    }

    return &skins_[handle.index()];
}

auto Drawlist::GetMotion(MotionId handle) const -> const Motion * {
    if (handle.index() >= motions_.size()) {
        return nullptr;
    }

    return &motions_[handle.index()];
}

auto Drawlist::FindNamedNode(std::string_view name) const -> std::optional<NodeId> {
    auto iter = std::find_if(
        named_nodes_.begin(), named_nodes_.end(), [&](const auto &node) { return node.GetName() == name; });

    if (iter == named_nodes_.end()) {
        return std::nullopt;
    }

    return NodeId{static_cast<uint32_t>(std::distance(named_nodes_.begin(), iter))};
}

auto Drawlist::AddNamedNode(std::string_view name) -> std::optional<NodeId> {
    const auto node = FindNamedNode(name);
    if (node.has_value()) {
        return node;
    }

    named_nodes_.emplace_back(Node{this, name});
    return NodeId{static_cast<uint32_t>(named_nodes_.size() - 1)};
}

auto Drawlist::AddSkin(std::span<const glm::fmat4x4> skin_matrices) -> std::optional<SkinId> {
    auto skinning_buffer_id = device_->CreateSkinningBuffer();
    device_->UpdateSkinningBuffer(skinning_buffer_id, skin_matrices);

    skins_.emplace_back(Skin{this, skinning_buffer_id});
    return SkinId{static_cast<uint32_t>(skins_.size() - 1)};
}

auto Drawlist::MakeMotionList() const -> std::vector<MotionId> {
    std::vector<MotionId> motions;
    motions.reserve(motions_.size());

    for (uint32_t i = 0; i < motions_.size(); ++i) {
        motions.emplace_back(MotionId{i});
    }

    return motions;
}

} // namespace render
