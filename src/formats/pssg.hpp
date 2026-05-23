#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <span>
#include <vector>
#include <optional>

namespace pssg {

enum class ScePssgElementType {
    eUnknown,
    eNone,
    eFloat,
    eInt,
    eUnsignedInt,
    eShort,
    eUnsignedShort,
    eByte,
    eHalf,
};

enum class ScePssgAttributeType {
    eUnknown,
    eInt,
    eString,
    eFloat,
    eFloat2,
    eFloat3,
    eFloat4,
};

struct ScePssgKnownSchemaAttribute {
    std::string_view element;
    std::string_view name;
    ScePssgAttributeType type;
};

struct ScePssgKnownSchemaElement {
    std::string_view name;
    ScePssgElementType type;
};

/// https://github.com/EgoEngineModding/Ego-Engine-Modding
// clang-format off
constexpr static auto kKnownElementTypes = std::to_array<ScePssgKnownSchemaElement>({
    ScePssgKnownSchemaElement{"BOUNDINGBOX",                                ScePssgElementType::eFloat},
    ScePssgKnownSchemaElement{"BUNDLENODE",                                 ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"CGSTREAM",                                   ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"CUBEMAPTEXTURE",                             ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"DATA",                                       ScePssgElementType::eByte},
    ScePssgKnownSchemaElement{"DATABLOCK",                                  ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"DATABLOCKDATA",                              ScePssgElementType::eByte},
    ScePssgKnownSchemaElement{"DATABLOCKSTREAM",                            ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"PSSGDATABASE",                               ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"FEATLASINFO",                                ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"FEATLASINFODATA",                            ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"INDEXSOURCEDATA",                            ScePssgElementType::eByte},
    ScePssgKnownSchemaElement{"INVERSEBINDMATRIX",                          ScePssgElementType::eFloat},
    ScePssgKnownSchemaElement{"JOINTNODE",                                  ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"LAYER",                                      ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"LIBRARY",                                    ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"LODNODE",                                    ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"LODRENDERINSTANCELIST",                      ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"LODRENDERINSTANCES",                         ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"LODRENDERNODE",                              ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"LODSKINNODE",                                ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"LODVISIBLERENDERNODE",                       ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"MATRIXPALETTEBUNDLENODE",                    ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"MATRIXPALETTEJOINTNODE",                     ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"MATRIXPALETTEJOINTRENDERINSTANCE",           ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"MATRIXPALETTENODE",                          ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"MATRIXPALETTERENDERINSTANCE",                ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"MATRIXPALETTESKINJOINT",                     ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"MODIFIERNETWORKINSTANCE",                    ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"MODIFIERNETWORKINSTANCECOMPILE",             ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"MODIFIERNETWORKINSTANCEMODIFIERINPUT",       ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"MODIFIERNETWORKINSTANCEUNIQUEMODIFIERINPUT", ScePssgElementType::eUnsignedInt},
    ScePssgKnownSchemaElement{"PNSTRING",                                   ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"NODE",                                       ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"XXX",                                        ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"RENDERDATASOURCE",                           ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"RENDERINDEXSOURCE",                          ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"RENDERINSTANCE",                             ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"RENDERINSTANCESOURCE",                       ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"RENDERINTERFACEBOUND",                       ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"RENDERNODE",                                 ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"RENDERSTREAM",                               ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"RENDERSTREAMINSTANCE",                       ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"ROOTNODE",                                   ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"SEGMENTSET",                                 ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"SHADERGROUP",                                ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"SHADERGROUPPASS",                            ScePssgElementType::eByte},
    ScePssgKnownSchemaElement{"SHADERINPUT",                                ScePssgElementType::eFloat},
    ScePssgKnownSchemaElement{"SHADERINPUTDEFINITION",                      ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"SHADERINSTANCE",                             ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"SHADERPROGRAM",                              ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"SHADERPROGRAMCODE",                          ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"SHADERPROGRAMCODEBLOCK",                     ScePssgElementType::eByte},
    ScePssgKnownSchemaElement{"SHADERSTREAMDEFINITION",                     ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"SKELETON",                                   ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"SKINJOINT",                                  ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"SKINNODE",                                   ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"TEXTURE",                                    ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"TEXTUREIMAGE",                               ScePssgElementType::eByte},
    ScePssgKnownSchemaElement{"TEXTUREIMAGEBLOCK",                          ScePssgElementType::eNone},
    ScePssgKnownSchemaElement{"TEXTUREIMAGEBLOCKDATA",                      ScePssgElementType::eByte},
    ScePssgKnownSchemaElement{"TEXTUREMIPMAP",                              ScePssgElementType::eByte},
    ScePssgKnownSchemaElement{"TRANSFORM",                                  ScePssgElementType::eFloat},
    ScePssgKnownSchemaElement{"VISIBLERENDERNODE",                          ScePssgElementType::eNone},
});
// clang-format on

constexpr static auto kKnownAttribTypes = std::to_array<ScePssgKnownSchemaAttribute>({
    ScePssgKnownSchemaAttribute{"CGSTREAM", "cgStreamName", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"CGSTREAM", "cgStreamDataType", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"CGSTREAM", "cgStreamRenderType", ScePssgAttributeType::eString},

    ScePssgKnownSchemaAttribute{"DATABLOCK", "streamCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"DATABLOCK", "size", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"DATABLOCK", "elementCount", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"DATABLOCKSTREAM", "renderType", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"DATABLOCKSTREAM", "dataType", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"DATABLOCKSTREAM", "offset", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"DATABLOCKSTREAM", "stride", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"PSSGDATABASE", "creator", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"PSSGDATABASE", "creationMachine", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"PSSGDATABASE", "creationDate", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"PSSGDATABASE", "scale", ScePssgAttributeType::eFloat3},
    ScePssgKnownSchemaAttribute{"PSSGDATABASE", "up", ScePssgAttributeType::eFloat3},

    ScePssgKnownSchemaAttribute{"FEATLASINFO", "atlasname", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"FEATLASINFO", "numberatlastextures", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"FEATLASINFODATA", "texturename", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"FEATLASINFODATA", "u0", ScePssgAttributeType::eFloat},
    ScePssgKnownSchemaAttribute{"FEATLASINFODATA", "v0", ScePssgAttributeType::eFloat},
    ScePssgKnownSchemaAttribute{"FEATLASINFODATA", "u1", ScePssgAttributeType::eFloat},
    ScePssgKnownSchemaAttribute{"FEATLASINFODATA", "v1", ScePssgAttributeType::eFloat},

    ScePssgKnownSchemaAttribute{"LAYER", "name", ScePssgAttributeType::eString},

    ScePssgKnownSchemaAttribute{"LIBRARY", "type", ScePssgAttributeType::eString},

    ScePssgKnownSchemaAttribute{"LODRENDERINSTANCELIST", "lod", ScePssgAttributeType::eFloat},

    ScePssgKnownSchemaAttribute{"LODRENDERINSTANCES", "lodCount", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"MATRIXPALETTEJOINTNODE", "matrixPalette", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"MATRIXPALETTEJOINTNODE", "jointID", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"MATRIXPALETTEJOINTRENDERINSTANCE", "streamOffset", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{
        "MATRIXPALETTEJOINTRENDERINSTANCE", "elementCountFromOffset", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MATRIXPALETTEJOINTRENDERINSTANCE", "indexOffset", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{
        "MATRIXPALETTEJOINTRENDERINSTANCE", "indicesCountFromOffset", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MATRIXPALETTEJOINTRENDERINSTANCE", "jointID", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"MATRIXPALETTENODE", "jointCount", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"MATRIXPALETTERENDERINSTANCE", "streamOffset", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MATRIXPALETTERENDERINSTANCE", "elementCountFromOffset", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MATRIXPALETTERENDERINSTANCE", "indexOffset", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MATRIXPALETTERENDERINSTANCE", "indicesCountFromOffset", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MATRIXPALETTERENDERINSTANCE", "jointCount", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"MATRIXPALETTESKINJOINT", "joint", ScePssgAttributeType::eString},

    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCE", "dynamicStreamCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCE", "modifierInputCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCE", "parameterCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCE", "modifierCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCE", "packetModifierCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCE", "network", ScePssgAttributeType::eString},

    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCECOMPILE", "uniqueInputCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCECOMPILE", "packetCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCECOMPILE", "maxElementCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCECOMPILE", "memorySizeForProcess", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCECOMPILE", "maxPacketOutputSize", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCECOMPILE", "maxTemporaryBufferSize", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCECOMPILE", "stateBlockBufferSize", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCECOMPILE", "totalInputPacketSize", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCECOMPILE", "packetSizeCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{
        "MODIFIERNETWORKINSTANCECOMPILE", "packetModifierInputCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCECOMPILE", "infoPacketSize", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCEMODIFIERINPUT", "source", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"MODIFIERNETWORKINSTANCEMODIFIERINPUT", "stream", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"PNSTRING", "data", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"PNSTRING", "size", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"NODE", "stopTraversal", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"NODE", "nickname", ScePssgAttributeType::eString},

    ScePssgKnownSchemaAttribute{"XXX", "id", ScePssgAttributeType::eString},

    ScePssgKnownSchemaAttribute{"RENDERDATASOURCE", "streamCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"RENDERDATASOURCE", "packetCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"RENDERDATASOURCE", "packetListCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"RENDERDATASOURCE", "primitive", ScePssgAttributeType::eString},

    ScePssgKnownSchemaAttribute{"RENDERINDEXSOURCE", "primitive", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"RENDERINDEXSOURCE", "minimumIndex", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"RENDERINDEXSOURCE", "maximumIndex", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"RENDERINDEXSOURCE", "format", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"RENDERINDEXSOURCE", "count", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"RENDERINSTANCE", "streamCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"RENDERINSTANCE", "shader", ScePssgAttributeType::eString},

    ScePssgKnownSchemaAttribute{"RENDERINSTANCESOURCE", "source", ScePssgAttributeType::eString},

    ScePssgKnownSchemaAttribute{"RENDERINTERFACEBOUND", "localData", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"RENDERINTERFACEBOUND", "isRenderTarget", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"RENDERINTERFACEBOUND", "allocateSystem", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"RENDERINTERFACEBOUND", "prioritizeRead", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"RENDERINTERFACEBOUND", "automaticBind", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"RENDERINTERFACEBOUND", "discardLocalAfterBind", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"RENDERSTREAM", "dataBlock", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"RENDERSTREAM", "subStream", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"RENDERSTREAMINSTANCE", "sourceCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"RENDERSTREAMINSTANCE", "indices", ScePssgAttributeType::eString},

    ScePssgKnownSchemaAttribute{"SEGMENTSET", "segmentCount", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"SHADERGROUP", "parameterCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUP", "parameterSavedCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUP", "parameterStreamCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUP", "instancesRequireSorting", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUP", "defaultRenderSortPriority", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUP", "passCount", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "vertexProgram", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "fragmentProgram", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "hullProgram", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "domainProgram", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "blendEnable", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "blendSource", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "blendDest", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "blendOp", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "separateAlphaBlendEnable", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "blendSourceAlpha", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "blendDestAlpha", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "blendOpAlpha", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "alphaTestEnable", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "alphaTestFunc", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "alphaTestRef", ScePssgAttributeType::eFloat},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "alphaToCoverageEnable", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "alphaToCoverageLevel", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "depthTestEnable", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "depthTestFunc", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "depthMaskEnable", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "cullFaceType", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "polyFillType", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "polyOffsetEnable", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "polyOffsetFactor", ScePssgAttributeType::eFloat},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "polyOffsetUnits", ScePssgAttributeType::eFloat},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "colorMaskRed", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "colorMaskGreen", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "colorMaskBlue", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "colorMaskAlpha", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "stencilMode", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "2SidedStencilFrontFunc", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "2SidedStencilFrontRef", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "2SidedStencilFrontMask", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "2SidedStencilFrontFailOp", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "2SidedStencilFrontZFailOp", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "2SidedStencilFrontZPassOp", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "2SidedStencilFrontStencilMask", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "2SidedStencilBackFunc", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "2SidedStencilBackRef", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "2SidedStencilBackMask", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "2SidedStencilBackFailOp", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "2SidedStencilBackZFailOp", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "2SidedStencilBackZPassOp", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "2SidedStencilBackStencilMask", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "normalizeEnable", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "passConfigMaskLow", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "passConfigMaskHigh", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "polySmoothEnable", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERGROUPPASS", "coupleVertexAndPixelProgram", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"SHADERINPUT", "parameterID", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERINPUT", "type", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERINPUT", "format", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERINPUT", "custom", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERINPUT", "texture", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERINPUT", "light", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERINPUT", "object", ScePssgAttributeType::eString},

    ScePssgKnownSchemaAttribute{"SHADERINPUTDEFINITION", "name", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERINPUTDEFINITION", "type", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERINPUTDEFINITION", "format", ScePssgAttributeType::eString},

    ScePssgKnownSchemaAttribute{"SHADERINSTANCE", "shaderGroup", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERINSTANCE", "parameterCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERINSTANCE", "parameterSavedCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERINSTANCE", "renderSortPriority", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"SHADERPROGRAM", "codeCount", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"SHADERPROGRAMCODE", "codeSize", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERPROGRAMCODE", "codeType", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERPROGRAMCODE", "profileType", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERPROGRAMCODE", "profile", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERPROGRAMCODE", "codeEntry", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERPROGRAMCODE", "parameterCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SHADERPROGRAMCODE", "streamCount", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"SHADERSTREAMDEFINITION", "renderTypeName", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SHADERSTREAMDEFINITION", "name", ScePssgAttributeType::eString},

    ScePssgKnownSchemaAttribute{"SKELETON", "matrixCount", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"SKINJOINT", "joint", ScePssgAttributeType::eString},

    ScePssgKnownSchemaAttribute{"SKINNODE", "jointCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"SKINNODE", "skeleton", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"SKINNODE", "updateBounds", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"TEXTURE", "width", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "height", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "depth", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "texelFormat", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"TEXTURE", "transient", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "wrapS", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "wrapT", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "wrapR", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "minFilter", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "magFilter", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "automipmap", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "numberMipMapLevels", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "msaaType", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "gammaRemapR", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "gammaRemapG", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "gammaRemapB", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "gammaRemapA", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "enableCompare", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "maxAnisotropy", ScePssgAttributeType::eFloat},
    ScePssgKnownSchemaAttribute{"TEXTURE", "lodBias", ScePssgAttributeType::eFloat},
    ScePssgKnownSchemaAttribute{"TEXTURE", "enableVertexTexture", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "borderColor", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "imageBlockCount", ScePssgAttributeType::eInt},
    ScePssgKnownSchemaAttribute{"TEXTURE", "mipZeroAbsent", ScePssgAttributeType::eInt},

    ScePssgKnownSchemaAttribute{"TEXTUREIMAGEBLOCK", "typename", ScePssgAttributeType::eString},
    ScePssgKnownSchemaAttribute{"TEXTUREIMAGEBLOCK", "size", ScePssgAttributeType::eInt},
});

struct ScePssgSchemaAttribute {
    uint32_t id;
    uint32_t element_id;
    std::string name;

    ScePssgAttributeType type = ScePssgAttributeType::eUnknown;
};

struct ScePssgSchemaElement {
    uint32_t id;
    std::string name;

    ScePssgElementType type = ScePssgElementType::eUnknown;
};

struct ScePssgSchema {
    std::vector<ScePssgSchemaElement> elements;
    std::vector<ScePssgSchemaAttribute> attributes;
};

struct ScePssgAttribute {
    uint32_t id;
    std::vector<uint8_t> value;
};

struct ScePssgElement {
    uint32_t id;
    std::vector<ScePssgAttribute> attributes;

    std::optional<uint32_t> parent;
    std::vector<uint32_t> children;

    std::vector<uint8_t> value;
};

struct ScePssgFile {
    ScePssgSchema schema;
    std::vector<ScePssgElement> elements;

    uint32_t root_element_idx;
};

class PssgLogger {
public:
    virtual ~PssgLogger() = default;
    virtual auto log(std::string_view log_message) const -> void = 0;
};

auto LoadFromMemory(const std::span<const uint8_t> buffer, const PssgLogger *logger = nullptr) -> ScePssgFile;
auto CheckHeader(std::span<const uint8_t> buffer, const PssgLogger *logger = nullptr) -> bool;

} // namespace pssg