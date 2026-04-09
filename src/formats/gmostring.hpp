#pragma once
#include <fmt/format.h>

#include "formats/gmo.hpp"

namespace gmo {

constexpr auto ToString(GmoFCurveInterpolation value) -> std::string_view {
    switch (value) {
    case gmo::GmoFCurveInterpolation::eConstant:
        return "eConstant";
    case gmo::GmoFCurveInterpolation::eLinear:
        return "eLinear";
    case gmo::GmoFCurveInterpolation::eHermite:
        return "eHermite";
    case gmo::GmoFCurveInterpolation::eCubic:
        return "eCubic";
    case gmo::GmoFCurveInterpolation::eSpherical:
        return "eSpherical";
    default:
        break;
    }

    return "eUnknown";
}

constexpr auto ToString(GmoAnimationTarget value) -> std::string_view {
    switch (value) {
    case gmo::GmoAnimationTarget::eBone:
        return "eBone";
    case gmo::GmoAnimationTarget::eMaterial:
        return "eMaterial";
    default:
        break;
    }
    return "eUnknown";
}

constexpr auto ToString(GmoBoneFlags value) -> std::string_view {
    switch (value) {
    case GmoBoneFlags::eBoneNone:
        return "eBoneNone";
    case GmoBoneFlags::eBoneHasTranslation:
        return "eBoneHasTranslation";
    case GmoBoneFlags::eBoneHasRotation:
        return "eBoneHasRotation";
    case GmoBoneFlags::eBoneHasScale1:
        return "eBoneHasScale1";
    case GmoBoneFlags::eBoneHasScale2:
        return "eBoneHasScale2";
    case GmoBoneFlags::eBoneHasScale3:
        return "eBoneHasScale3";
    case GmoBoneFlags::eBoneHasMultMatrix:
        return "eBoneHasMultMatrix";
    case GmoBoneFlags::eBoneHasPivot:
        return "eBoneHasPivot";
    case GmoBoneFlags::eBoneHasMorphWeights:
        return "eBoneHasMorphWeights";
    case GmoBoneFlags::eBoneHasVisibility:
        return "eBoneHasVisibility";
    case GmoBoneFlags::eBoneHasBoundingBox:
        return "eBoneHasBoundingBox";
    case GmoBoneFlags::eBoneHasBlendBones:
        return "eBoneHasBlendBones";
    default:
        break;
    }
    return "eUnknown";
}

constexpr auto ToString(GmoVertexArrayFlags value) -> std::string_view {
    switch (value) {
    case GmoVertexArrayFlags::eNone:
        return "eNone";
    case GmoVertexArrayFlags::eHasPositions:
        return "eHasPositions";
    case GmoVertexArrayFlags::eHasNormals:
        return "eHasNormals";
    case GmoVertexArrayFlags::eHasUvs:
        return "eHasUvs";
    case GmoVertexArrayFlags::eHasWeights:
        return "eHasWeights";
    case GmoVertexArrayFlags::eHasColor:
        return "eHasColor";
    default:
        break;
    }
    return "eUnknown";
}

constexpr auto ToString(GmoDrawPrimitive value) -> std::string_view {
    switch (value) {
    case GmoDrawPrimitive::ePoints:
        return "ePoints";
    case GmoDrawPrimitive::eLines:
        return "eLines";
    case GmoDrawPrimitive::eLineStrip:
        return "eLineStrip";
    case GmoDrawPrimitive::eTriangles:
        return "eTriangles";
    case GmoDrawPrimitive::eTriangleStrip:
        return "eTriangleStrip";
    case GmoDrawPrimitive::eTriangleFan:
        return "eTriangleFan";
    case GmoDrawPrimitive::eRectangles:
        return "eRectangles";
    default:
        break;
    }
    return "eUnknown";
}

constexpr auto ToString(GmoMaterialLayerFlags value) -> std::string_view {
    switch (value) {
    case GmoMaterialLayerFlags::eNone:
        return "eNone";
    case GmoMaterialLayerFlags::eHasTextureCrop:
        return "eHasTextureCrop";
    default:
        break;
    }
    return "eUnknown";
}

constexpr auto ToString(GmoMaterialLayerMapType value) -> std::string_view {
    switch (value) {
    case GmoMaterialLayerMapType::eNone:
        return "eNone";
    case GmoMaterialLayerMapType::eDiffuse:
        return "eDiffuse";
    case GmoMaterialLayerMapType::eSpecular:
        return "eSpecular";
    case GmoMaterialLayerMapType::eEmission:
        return "eEmission";
    case GmoMaterialLayerMapType::eAmbient:
        return "eAmbient";
    case GmoMaterialLayerMapType::eReflection:
        return "eReflection";
    case GmoMaterialLayerMapType::eRefraction:
        return "eRefraction";
    case GmoMaterialLayerMapType::eBump:
        return "eBump";
    default:
        break;
    }
    return "eUnknown";
}

constexpr auto ToString(GmoBlendOperator value) -> std::string_view {
    switch (value) {
    case GmoBlendOperator::eAdd:
        return "eAdd";
    case GmoBlendOperator::eSubtract:
        return "eSubtract";
    case GmoBlendOperator::eReverseSubtract:
        return "eReverseSubtract";
    case GmoBlendOperator::eMin:
        return "eMin";
    case GmoBlendOperator::eMax:
        return "eMax";
    case GmoBlendOperator::eAbs:
        return "eAbs";
    default:
        break;
    }
    return "eUnknown";
}

constexpr auto ToString(GmoBlendFunction value) -> std::string_view {
    switch (value) {
    case GmoBlendFunction::eSrcColor:
        return "eSrcColor";
    case GmoBlendFunction::eOneMinusSrcColor:
        return "eOneMinusSrcColor";
    case GmoBlendFunction::eDstColor:
        return "eDstColor";
    case GmoBlendFunction::eOneMinusDstColor:
        return "eOneMinusDstColor";
    case GmoBlendFunction::eSrcAlpha:
        return "eSrcAlpha";
    case GmoBlendFunction::eOneMinusSrcAlpha:
        return "eOneMinusSrcAlpha";
    case GmoBlendFunction::eDstAlpha:
        return "eDstAlpha";
    case GmoBlendFunction::eOneMinusDstAlpha:
        return "eOneMinusDstAlpha";
    case GmoBlendFunction::eDoubleSrcAlpha:
        return "eDoubleSrcAlpha";
    case GmoBlendFunction::eOneMinusDoubleSrcAlpha:
        return "eOneMinusDoubleSrcAlpha";
    case GmoBlendFunction::eDoubleDstAlpha:
        return "eDoubleDstAlpha";
    case GmoBlendFunction::eOneMinusDoubleDstAlpha:
        return "eOneMinusDoubleDstAlpha";
    case GmoBlendFunction::eFixValue:
        return "eFixValue";
    default:
        break;
    }
    return "eUnknown";
}

constexpr auto ToString(GmoMaterialFlags value) -> std::string_view {
    switch (value) {
    case GmoMaterialFlags::eNone:
        return "eNone";
    case GmoMaterialFlags::eEnableLighting:
        return "eEnableLighting";
    case GmoMaterialFlags::eEnableFog:
        return "eEnableFog";
    case GmoMaterialFlags::eEnableCullFace:
        return "eEnableCullFace";
    case GmoMaterialFlags::eEnableDepthTest:
        return "eEnableDepthTest";
    case GmoMaterialFlags::eEnableDepthMask:
        return "eEnableDepthMask";
    case GmoMaterialFlags::eEnableAlphaTest:
        return "eEnableAlphaTest";
    case GmoMaterialFlags::eEnableAlphaMask:
        return "eEnableAlphaMask";
    case GmoMaterialFlags::eHasDiffuse:
        return "eHasDiffuse";
    case GmoMaterialFlags::eHasSpecular:
        return "eHasSpecular";
    case GmoMaterialFlags::eHasEmission:
        return "eHasEmission";
    case GmoMaterialFlags::eHasAmbient:
        return "eHasAmbient";
    case GmoMaterialFlags::eHasReflection:
        return "eHasReflection";
    case GmoMaterialFlags::eHasRefraction:
        return "eHasRefraction";
    default:
        break;
    }
    return "eUnknown";
}

constexpr auto ToString(GmoModelFlags value) -> std::string_view {
    switch (value) {
    case GmoModelFlags::eModelNone:
        return "eModelNone";
    case GmoModelFlags::eModelHasBoundingBox:
        return "eModelHasBoundingBox";
    case GmoModelFlags::eModelHasVertexOffset:
        return "eModelHasVertexOffset";
    case GmoModelFlags::eModelHasTextureOffset:
        return "eModelHasTextureOffset";
    default:
        break;
    }
    return "eUnknown";
}

inline auto ToString(GmoMaterialColor value) -> std::string {
    switch (value) {
    case eGmoMaterialColorBlack:
        return "eGmoMaterialColorBlack";
    case eGmoMaterialColorDiffuse:
        return "eGmoMaterialColorDiffuse";
    case eGmoMaterialColorSpecular:
        return "eGmoMaterialColorSpecular";
    case eGmoMaterialColorEmission:
        return "eGmoMaterialColorEmission";
    case eGmoMaterialColorAmbient:
        return "eGmoMaterialColorAmbient";
    case eGmoMaterialColorReflection:
        return "eGmoMaterialColorReflection";
    case eGmoMaterialColorCount:
        return "eGmoMaterialColorCount";
    default:
        break;
    }
    return fmt::format("eUnknown({:#010x})", static_cast<uint32_t>(value));
}

inline auto ToString(GmoAnimationProperty value) -> std::string {
    switch (value) {
    case eAnimBoneTranslate:
        return "eAnimBoneTranslate";
    case eAnimBoneRotateQ:
        return "eAnimBoneRotateQ";
    case eAnimBoneRotateZYX:
        return "eAnimBoneRotateZYX";
    case eAnimBoneRotateYXZ:
        return "eAnimBoneRotateYXZ";
    case eAnimBoneScale1:
        return "eAnimBoneScale1";
    case eAnimBoneScale2:
        return "eAnimBoneScale2";
    case eAnimBoneScale3:
        return "eAnimBoneScale3";
    case eAnimBoneMultMatrix:
        return "eAnimBoneMultMatrix";
    case eAnimBoneMorphWeights:
        return "eAnimBoneMorphWeights";
    case eAnimBoneMorphIndex:
        return "eAnimBoneMorphIndex";
    case eAnimBoneVisibility:
        return "eAnimBoneVisibility";
    case eAnimMaterialDiffuse:
        return "eAnimMaterialDiffuse";
    case eAnimMaterialSpecular:
        return "eAnimMaterialSpecular";
    case eAnimMaterialEmission:
        return "eAnimMaterialEmission";
    case eAnimMaterialAmbient:
        return "eAnimMaterialAmbient";
    case eAnimMaterialReflection:
        return "eAnimMaterialReflection";
    case eAnimMaterialRefraction:
        return "eAnimMaterialRefraction";
    case eAnimMaterialBump:
        return "eAnimMaterialBump";
    case eAnimMaterialTextureCrop:
        return "eAnimMaterialTextureCrop";
    default:
        break;
    }
    return fmt::format("eUnknown({:#010x})", static_cast<uint32_t>(value));
}

} // namespace gmo
