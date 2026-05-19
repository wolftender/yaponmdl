#pragma once
#include <span>
#include <string_view>

namespace memoryresource {

auto GetVertShaderTexture() -> std::string_view;
auto GetFragShaderTexture() -> std::string_view;
auto GetFragShaderChecker() -> std::string_view;
auto GetFragShaderMesh() -> std::string_view;
auto GetVertShaderStaticMesh() -> std::string_view;
auto GetVertShaderSkinnedMesh() -> std::string_view;
auto GetVertShaderPost() -> std::string_view;
auto GetFragShaderPost() -> std::string_view;
auto GetVertShaderText() -> std::string_view;
auto GetFragShaderText() -> std::string_view;
auto GetVertShaderGrid() -> std::string_view;
auto GetFragShaderGrid() -> std::string_view;

auto GetFontOpensansTtf() -> std::span<const uint8_t>;

} // namespace memoryresource
