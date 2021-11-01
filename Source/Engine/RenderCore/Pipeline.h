/*

Angie Engine Source Code

MIT License

Copyright (C) 2017-2021 Alexander Samusev.

This file is part of the Angie Engine Source Code.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#pragma once

#include "DeviceObject.h"
#include "StaticLimits.h"
#include "ShaderModule.h"
#include "Texture.h"
#include "Buffer.h"

namespace RenderCore
{

enum
{
    DEFAULT_STENCIL_READ_MASK  = 0xff,
    DEFAULT_STENCIL_WRITE_MASK = 0xff
};

//
// Blending state
//

enum BLEND_OP : uint8_t
{
    BLEND_OP_ADD,              /// Rr=RssR+RddR Gr=GssG+GddG Br=BssB+BddB 	Ar=AssA+AddA
    BLEND_OP_SUBTRACT,         /// Rr=RssR−RddR Gr=GssG−GddG Br=BssB−BddB 	Ar=AssA−AddA
    BLEND_OP_REVERSE_SUBTRACT, /// Rr=RddR−RssR Gr=GddG−GssG Br=BddB−BssB 	Ar=AddA−AssA
    BLEND_OP_MIN,              /// Rr=min(Rs,Rd) Gr=min(Gs,Gd) Br=min(Bs,Bd) 	Ar=min(As,Ad)
    BLEND_OP_MAX               /// Rr=max(Rs,Rd) Gr=max(Gs,Gd) Br=max(Bs,Bd) 	Ar=max(As,Ad)
};

enum BLEND_FUNC : uint8_t
{
    BLEND_FUNC_ZERO, /// ( 0,  0,  0,  0 )
    BLEND_FUNC_ONE,  /// ( 1,  1,  1,  1 )

    BLEND_FUNC_SRC_COLOR,     /// ( Rs0 / kr,   Gs0 / kg,   Bs0 / kb,   As0 / ka )
    BLEND_FUNC_INV_SRC_COLOR, /// ( 1,  1,  1,  1 ) - ( Rs0 / kr,   Gs0 / kg,   Bs0 / kb,   As0 / ka )

    BLEND_FUNC_DST_COLOR,     /// ( Rd0 / kr,   Gd0 / kg,   Bd0 / kb,   Ad0 / ka )
    BLEND_FUNC_INV_DST_COLOR, /// ( 1,  1,  1,  1 ) - ( Rd0 / kr,   Gd0 / kg,   Bd0 / kb,   Ad0 / ka )

    BLEND_FUNC_SRC_ALPHA,     /// ( As0 / kA,   As0 / kA,   As0 / kA,   As0 / kA )
    BLEND_FUNC_INV_SRC_ALPHA, /// ( 1,  1,  1,  1 ) - ( As0 / kA,   As0 / kA,   As0 / kA,   As0 / kA )

    BLEND_FUNC_DST_ALPHA,     /// ( Ad / kA,    Ad / kA,    Ad / kA,    Ad / kA )
    BLEND_FUNC_INV_DST_ALPHA, /// ( 1,  1,  1,  1 ) - ( Ad / kA,    Ad / kA,    Ad / kA,    Ad / kA )

    BLEND_FUNC_CONSTANT_COLOR,     /// ( Rc, Gc, Bc, Ac )
    BLEND_FUNC_INV_CONSTANT_COLOR, /// ( 1,  1,  1,  1 ) - ( Rc, Gc, Bc, Ac )

    BLEND_FUNC_CONSTANT_ALPHA,     /// ( Ac, Ac, Ac, Ac )
    BLEND_FUNC_INV_CONSTANT_ALPHA, /// ( 1,  1,  1,  1 ) - ( Ac, Ac, Ac, Ac )

    BLEND_FUNC_SRC_ALPHA_SATURATE, /// ( i,  i,  i,  1 )

    BLEND_FUNC_SRC1_COLOR,     /// ( Rs1 / kR,   Gs1 / kG,   Bs1 / kB,   As1 / kA )
    BLEND_FUNC_INV_SRC1_COLOR, /// ( 1,  1,  1,  1 ) - ( Rs1 / kR,   Gs1 / kG,   Bs1 / kB,   As1 / kA )

    BLEND_FUNC_SRC1_ALPHA,    /// ( As1 / kA,   As1 / kA,   As1 / kA,   As1 / kA )
    BLEND_FUNC_INV_SRC1_ALPHA /// ( 1,  1,  1,  1 ) - ( As1 / kA,   As1 / kA,   As1 / kA,   As1 / kA )
};

enum BLENDING_PRESET : uint8_t
{
    BLENDING_NO_BLEND,
    BLENDING_ALPHA,
    BLENDING_PREMULTIPLIED_ALPHA,
    BLENDING_COLOR_ADD,
    BLENDING_MULTIPLY,
    BLENDING_SOURCE_TO_DEST,
    BLENDING_ADD_MUL,
    BLENDING_ADD_ALPHA,

    BLENDING_MAX_PRESETS
};

enum LOGIC_OP : uint8_t
{
    LOGIC_OP_COPY,
    LOGIC_OP_COPY_INV,
    LOGIC_OP_CLEAR,
    LOGIC_OP_SET,
    LOGIC_OP_NOOP,
    LOGIC_OP_INVERT,
    LOGIC_OP_AND,
    LOGIC_OP_NAND,
    LOGIC_OP_OR,
    LOGIC_OP_NOR,
    LOGIC_OP_XOR,
    LOGIC_OP_EQUIV,
    LOGIC_OP_AND_REV,
    LOGIC_OP_AND_INV,
    LOGIC_OP_OR_REV,
    LOGIC_OP_OR_INV
};

enum COLOR_WRITE_MASK : uint8_t
{
    COLOR_WRITE_DISABLED = 0,
    COLOR_WRITE_R_BIT    = 1,
    COLOR_WRITE_G_BIT    = 2,
    COLOR_WRITE_B_BIT    = 4,
    COLOR_WRITE_A_BIT    = 8,
    COLOR_WRITE_RGBA     = COLOR_WRITE_R_BIT | COLOR_WRITE_G_BIT | COLOR_WRITE_B_BIT | COLOR_WRITE_A_BIT,
    COLOR_WRITE_RGB      = COLOR_WRITE_R_BIT | COLOR_WRITE_G_BIT | COLOR_WRITE_B_BIT
};

struct SRenderTargetBlendingInfo
{
    struct Operation
    {
        BLEND_OP ColorRGB = BLEND_OP_ADD;
        BLEND_OP Alpha    = BLEND_OP_ADD;
    } Op;

    struct Function
    {
        BLEND_FUNC SrcFactorRGB   = BLEND_FUNC_ONE;
        BLEND_FUNC DstFactorRGB   = BLEND_FUNC_ZERO;
        BLEND_FUNC SrcFactorAlpha = BLEND_FUNC_ONE;
        BLEND_FUNC DstFactorAlpha = BLEND_FUNC_ZERO;
    } Func;

    bool             bBlendEnable = false;
    COLOR_WRITE_MASK ColorWriteMask = COLOR_WRITE_RGBA;

    // General blend equation:
    // if ( BlendEnable ) {
    //     ResultColorRGB = ( SourceColor.rgb * SrcFactorRGB ) Op.ColorRGB ( DestColor.rgb * DstFactorRGB )
    //     ResultAlpha    = ( SourceColor.a * SrcFactorAlpha ) Op.Alpha    ( DestColor.a * DstFactorAlpha )
    // } else {
    //     ResultColorRGB = SourceColor.rgb;
    //     ResultAlpha    = SourceColor.a;
    // }

    SRenderTargetBlendingInfo() = default;

    void SetBlendingPreset(BLENDING_PRESET _Preset);
};

AN_INLINE void SRenderTargetBlendingInfo::SetBlendingPreset(BLENDING_PRESET _Preset)
{
    switch (_Preset)
    {
        case BLENDING_ALPHA:
            bBlendEnable      = true;
            ColorWriteMask    = COLOR_WRITE_RGBA;
            Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_SRC_ALPHA;
            Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_INV_SRC_ALPHA;
            Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
            break;
        case BLENDING_PREMULTIPLIED_ALPHA:
            bBlendEnable      = true;
            ColorWriteMask    = COLOR_WRITE_RGBA;
            Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_ONE;
            Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_INV_SRC_ALPHA;
            Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
            break;
        case BLENDING_COLOR_ADD:
            bBlendEnable      = true;
            ColorWriteMask    = COLOR_WRITE_RGBA;
            Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_ONE;
            Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_ONE;
            Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
            break;
        case BLENDING_MULTIPLY:
            bBlendEnable      = true;
            ColorWriteMask    = COLOR_WRITE_RGBA;
            Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_DST_COLOR;
            Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_ZERO;
            Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
            break;
        case BLENDING_SOURCE_TO_DEST:
            bBlendEnable      = true;
            ColorWriteMask    = COLOR_WRITE_RGBA;
            Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_SRC_COLOR;
            Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_ONE;
            Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
            break;
        case BLENDING_ADD_MUL:
            bBlendEnable      = true;
            ColorWriteMask    = COLOR_WRITE_RGBA;
            Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_INV_DST_COLOR;
            Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_ONE;
            Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
            break;
        case BLENDING_ADD_ALPHA:
            bBlendEnable      = true;
            ColorWriteMask    = COLOR_WRITE_RGBA;
            Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_SRC_ALPHA;
            Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_ONE;
            Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
            break;
        case BLENDING_NO_BLEND:
        default:
            bBlendEnable      = false;
            ColorWriteMask    = COLOR_WRITE_RGBA;
            Func.SrcFactorRGB = Func.SrcFactorAlpha = BLEND_FUNC_ONE;
            Func.DstFactorRGB = Func.DstFactorAlpha = BLEND_FUNC_ZERO;
            Op.ColorRGB = Op.Alpha = BLEND_OP_ADD;
            break;
    }
}

struct SBlendingStateInfo
{
    bool                      bSampleAlphaToCoverage = false;
    bool                      bIndependentBlendEnable = false;
    LOGIC_OP                  LogicOp                 = LOGIC_OP_COPY;
    SRenderTargetBlendingInfo RenderTargetSlots[MAX_COLOR_ATTACHMENTS];

    SBlendingStateInfo() = default;

    bool operator==(SBlendingStateInfo const& Rhs) const
    {
        return std::memcmp(this, &Rhs, sizeof(*this)) == 0;
    }

    bool operator!=(SBlendingStateInfo const& Rhs) const
    {
        return !(operator==(Rhs));
    }
};

//
// Rasterizer state
//

enum POLYGON_FILL : uint8_t
{
    POLYGON_FILL_SOLID = 0,
    POLYGON_FILL_WIRE  = 1
};

enum POLYGON_CULL : uint8_t
{
    POLYGON_CULL_BACK     = 0,
    POLYGON_CULL_FRONT    = 1,
    POLYGON_CULL_DISABLED = 2
};

struct SRasterizerStateInfo
{
    POLYGON_FILL FillMode        = POLYGON_FILL_SOLID;
    POLYGON_CULL CullMode        = POLYGON_CULL_BACK;
    bool         bFrontClockwise = false;

    struct
    {
        /*
                       _
                      |       MaxDepthSlope x Slope + r * Bias,           if Clamp = 0 or NaN;
                      |
        DepthOffset = <   min(MaxDepthSlope x Slope + r * Bias, Clamp),   if Clamp > 0;
                      |
                      |_  max(MaxDepthSlope x Slope + r * Bias, Clamp),   if Clamp < 0.

        */
        float Slope = 0;
        int   Bias  = 0;
        float Clamp = 0;
    } DepthOffset;

    // If enabled, the −wc ≤ zc ≤ wc plane equation is ignored by view volume clipping
    // (effectively, there is no near or far plane clipping). See viewport->MinDepth, viewport->MaxDepth.
    bool bDepthClampEnable = false;
                                    
    bool bScissorEnable         = false;
    bool bMultisampleEnable     = false;
    bool bAntialiasedLineEnable = false;

    // If enabled, primitives are discarded after the optional transform feedback stage, but before rasterization
    bool bRasterizerDiscard     = false;

    SRasterizerStateInfo() = default;

    bool operator==(SRasterizerStateInfo const& Rhs) const
    {
        return std::memcmp(this, &Rhs, sizeof(*this)) == 0;
    }

    bool operator!=(SRasterizerStateInfo const& Rhs) const
    {
        return !(operator==(Rhs));
    }
};


//
// Depth-Stencil state
//

enum STENCIL_OP : uint8_t
{
    STENCIL_OP_KEEP     = 0,
    STENCIL_OP_ZERO     = 1,
    STENCIL_OP_REPLACE  = 2,
    STENCIL_OP_INCR_SAT = 3,
    STENCIL_OP_DECR_SAT = 4,
    STENCIL_OP_INVERT   = 5,
    STENCIL_OP_INCR     = 6,
    STENCIL_OP_DECR     = 7
};

struct SStencilTestInfo
{
    STENCIL_OP          StencilFailOp = STENCIL_OP_KEEP;
    STENCIL_OP          DepthFailOp   = STENCIL_OP_KEEP;
    STENCIL_OP          DepthPassOp   = STENCIL_OP_KEEP;
    COMPARISON_FUNCTION StencilFunc   = CMPFUNC_ALWAYS;
    //int              Reference = 0;

    SStencilTestInfo() = default;
};

struct SDepthStencilStateInfo
{
    bool                bDepthEnable     = true;
    bool                bDepthWrite      = true;
    COMPARISON_FUNCTION DepthFunc        = CMPFUNC_LESS;
    bool                bStencilEnable   = false;
    uint8_t             StencilReadMask  = DEFAULT_STENCIL_READ_MASK;
    uint8_t             StencilWriteMask = DEFAULT_STENCIL_WRITE_MASK;
    SStencilTestInfo    FrontFace;
    SStencilTestInfo    BackFace;

    SDepthStencilStateInfo() = default;

    bool operator==(SDepthStencilStateInfo const& Rhs) const
    {
        return std::memcmp(this, &Rhs, sizeof(*this)) == 0;
    }

    bool operator!=(SDepthStencilStateInfo const& Rhs) const
    {
        return !(operator==(Rhs));
    }
};


//
// Pipeline resource layout
//

enum IMAGE_ACCESS_MODE : uint8_t
{
    IMAGE_ACCESS_READ,
    IMAGE_ACCESS_WRITE,
    IMAGE_ACCESS_RW
};

struct SImageInfo
{
    IMAGE_ACCESS_MODE AccessMode = IMAGE_ACCESS_READ;
    TEXTURE_FORMAT    TextureFormat = TEXTURE_FORMAT_RGBA8; // FIXME: get texture format from texture?
};

struct SBufferInfo
{
    BUFFER_BINDING BufferBinding = BUFFER_BIND_CONSTANT;
};

struct SPipelineResourceLayout
{
    int                  NumSamplers = 0;
    struct SSamplerDesc* Samplers = nullptr;

    int         NumImages = 0;
    SImageInfo* Images = nullptr;

    int          NumBuffers = 0;
    SBufferInfo* Buffers = nullptr;

    SPipelineResourceLayout() = default;
};

//
// Vertex bindings and attributes
//

constexpr int VertexAttribType_NormalizedBit() { return 1 << 7; }

constexpr int VertexAttribType_CountBit(int Count) { return ((((Count)-1) & 3) << 5); }

constexpr int _5BitNumber(int Number) { return ((Number)&31); }

enum VERTEX_ATTRIB_COMPONENT : uint8_t
{
    COMPONENT_BYTE   = _5BitNumber(0),
    COMPONENT_UBYTE  = _5BitNumber(1),
    COMPONENT_SHORT  = _5BitNumber(2),
    COMPONENT_USHORT = _5BitNumber(3),
    COMPONENT_INT    = _5BitNumber(4),
    COMPONENT_UINT   = _5BitNumber(5),
    COMPONENT_HALF   = _5BitNumber(6),
    COMPONENT_FLOAT  = _5BitNumber(7),
    COMPONENT_DOUBLE = _5BitNumber(8)

    // Add here other types

    // MAX = 31
};

enum VERTEX_ATTRIB_TYPE : uint8_t
{
    /// Signed byte
    VAT_BYTE1  = COMPONENT_BYTE | VertexAttribType_CountBit(1),
    VAT_BYTE2  = COMPONENT_BYTE | VertexAttribType_CountBit(2),
    VAT_BYTE3  = COMPONENT_BYTE | VertexAttribType_CountBit(3),
    VAT_BYTE4  = COMPONENT_BYTE | VertexAttribType_CountBit(4),
    VAT_BYTE1N = VAT_BYTE1 | VertexAttribType_NormalizedBit(),
    VAT_BYTE2N = VAT_BYTE2 | VertexAttribType_NormalizedBit(),
    VAT_BYTE3N = VAT_BYTE3 | VertexAttribType_NormalizedBit(),
    VAT_BYTE4N = VAT_BYTE4 | VertexAttribType_NormalizedBit(),

    /// Unsigned byte
    VAT_UBYTE1  = COMPONENT_UBYTE | VertexAttribType_CountBit(1),
    VAT_UBYTE2  = COMPONENT_UBYTE | VertexAttribType_CountBit(2),
    VAT_UBYTE3  = COMPONENT_UBYTE | VertexAttribType_CountBit(3),
    VAT_UBYTE4  = COMPONENT_UBYTE | VertexAttribType_CountBit(4),
    VAT_UBYTE1N = VAT_UBYTE1 | VertexAttribType_NormalizedBit(),
    VAT_UBYTE2N = VAT_UBYTE2 | VertexAttribType_NormalizedBit(),
    VAT_UBYTE3N = VAT_UBYTE3 | VertexAttribType_NormalizedBit(),
    VAT_UBYTE4N = VAT_UBYTE4 | VertexAttribType_NormalizedBit(),

    /// Signed short (16 bit integer)
    VAT_SHORT1  = COMPONENT_SHORT | VertexAttribType_CountBit(1),
    VAT_SHORT2  = COMPONENT_SHORT | VertexAttribType_CountBit(2),
    VAT_SHORT3  = COMPONENT_SHORT | VertexAttribType_CountBit(3),
    VAT_SHORT4  = COMPONENT_SHORT | VertexAttribType_CountBit(4),
    VAT_SHORT1N = VAT_SHORT1 | VertexAttribType_NormalizedBit(),
    VAT_SHORT2N = VAT_SHORT2 | VertexAttribType_NormalizedBit(),
    VAT_SHORT3N = VAT_SHORT3 | VertexAttribType_NormalizedBit(),
    VAT_SHORT4N = VAT_SHORT4 | VertexAttribType_NormalizedBit(),

    /// Unsigned short (16 bit integer)
    VAT_USHORT1  = COMPONENT_USHORT | VertexAttribType_CountBit(1),
    VAT_USHORT2  = COMPONENT_USHORT | VertexAttribType_CountBit(2),
    VAT_USHORT3  = COMPONENT_USHORT | VertexAttribType_CountBit(3),
    VAT_USHORT4  = COMPONENT_USHORT | VertexAttribType_CountBit(4),
    VAT_USHORT1N = VAT_USHORT1 | VertexAttribType_NormalizedBit(),
    VAT_USHORT2N = VAT_USHORT2 | VertexAttribType_NormalizedBit(),
    VAT_USHORT3N = VAT_USHORT3 | VertexAttribType_NormalizedBit(),
    VAT_USHORT4N = VAT_USHORT4 | VertexAttribType_NormalizedBit(),

    /// 32-bit signed integer
    VAT_INT1  = COMPONENT_INT | VertexAttribType_CountBit(1),
    VAT_INT2  = COMPONENT_INT | VertexAttribType_CountBit(2),
    VAT_INT3  = COMPONENT_INT | VertexAttribType_CountBit(3),
    VAT_INT4  = COMPONENT_INT | VertexAttribType_CountBit(4),
    VAT_INT1N = VAT_INT1 | VertexAttribType_NormalizedBit(),
    VAT_INT2N = VAT_INT2 | VertexAttribType_NormalizedBit(),
    VAT_INT3N = VAT_INT3 | VertexAttribType_NormalizedBit(),
    VAT_INT4N = VAT_INT4 | VertexAttribType_NormalizedBit(),

    /// 32-bit unsigned integer
    VAT_UINT1  = COMPONENT_UINT | VertexAttribType_CountBit(1),
    VAT_UINT2  = COMPONENT_UINT | VertexAttribType_CountBit(2),
    VAT_UINT3  = COMPONENT_UINT | VertexAttribType_CountBit(3),
    VAT_UINT4  = COMPONENT_UINT | VertexAttribType_CountBit(4),
    VAT_UINT1N = VAT_UINT1 | VertexAttribType_NormalizedBit(),
    VAT_UINT2N = VAT_UINT2 | VertexAttribType_NormalizedBit(),
    VAT_UINT3N = VAT_UINT3 | VertexAttribType_NormalizedBit(),
    VAT_UINT4N = VAT_UINT4 | VertexAttribType_NormalizedBit(),

    /// 16-bit floating point
    VAT_HALF1 = COMPONENT_HALF | VertexAttribType_CountBit(1), // only with IsHalfFloatVertexSupported
    VAT_HALF2 = COMPONENT_HALF | VertexAttribType_CountBit(2), // only with IsHalfFloatVertexSupported
    VAT_HALF3 = COMPONENT_HALF | VertexAttribType_CountBit(3), // only with IsHalfFloatVertexSupported
    VAT_HALF4 = COMPONENT_HALF | VertexAttribType_CountBit(4), // only with IsHalfFloatVertexSupported

    /// 32-bit floating point
    VAT_FLOAT1 = COMPONENT_FLOAT | VertexAttribType_CountBit(1),
    VAT_FLOAT2 = COMPONENT_FLOAT | VertexAttribType_CountBit(2),
    VAT_FLOAT3 = COMPONENT_FLOAT | VertexAttribType_CountBit(3),
    VAT_FLOAT4 = COMPONENT_FLOAT | VertexAttribType_CountBit(4),

    /// 64-bit floating point
    VAT_DOUBLE1 = COMPONENT_DOUBLE | VertexAttribType_CountBit(1),
    VAT_DOUBLE2 = COMPONENT_DOUBLE | VertexAttribType_CountBit(2),
    VAT_DOUBLE3 = COMPONENT_DOUBLE | VertexAttribType_CountBit(3),
    VAT_DOUBLE4 = COMPONENT_DOUBLE | VertexAttribType_CountBit(4)
};

enum VERTEX_ATTRIB_MODE : uint8_t
{
    VAM_FLOAT,
    VAM_DOUBLE,
    VAM_INTEGER,
};

enum VERTEX_INPUT_RATE : uint8_t
{
    INPUT_RATE_PER_VERTEX   = 0,
    INPUT_RATE_PER_INSTANCE = 1
};

struct SVertexBindingInfo
{
    uint8_t           InputSlot = 0;                     /// vertex buffer binding
    uint32_t          Stride    = 0;                     /// vertex stride
    VERTEX_INPUT_RATE InputRate = INPUT_RATE_PER_VERTEX; /// per vertex / per instance
};

struct SVertexAttribInfo
{
    const char* SemanticName = "Undefined";
    uint32_t    Location     = 0;

    /// vertex buffer binding
    uint32_t           InputSlot = 0;
    VERTEX_ATTRIB_TYPE Type      = VAT_FLOAT1;

    /// float / double / integer
    VERTEX_ATTRIB_MODE Mode = VAM_FLOAT;

    /// Only for INPUT_RATE_PER_INSTANCE. The number of instances to draw using same
    /// per-instance data before advancing in the buffer by one element. This value must
    /// /// by 0 for an element that contains per-vertex data (InputRate = INPUT_RATE_PER_VERTEX)
    uint32_t InstanceDataStepRate = 0;

    /// attribute offset
    uint32_t Offset = 0;

    /// Number of vector components 1,2,3,4
    int NumComponents() const { return ((Type >> 5) & 3) + 1; }

    /// Type of vector components COMPONENT_BYTE, COMPONENT_SHORT, COMPONENT_HALF, COMPONENT_FLOAT, etc.
    VERTEX_ATTRIB_COMPONENT TypeOfComponent() const { return (VERTEX_ATTRIB_COMPONENT)_5BitNumber(Type); }

    /// Components are normalized
    bool IsNormalized() const { return !!(Type & VertexAttribType_NormalizedBit()); }
};

//
// Vertex attrubte to shader string helper
//

template <typename TString>
TString ShaderStringForVertexAttribs(SVertexAttribInfo const* _VertexAttribs, int _NumVertexAttribs)
{
    // TODO: modify for compile time?

    TString     s;
    const char* attribType;
    int         attribIndex = 0;
    char        location[16];

    const char* Types[4][4] = {
        {"float", "vec2", "vec3", "vec4"},     // Float types
        {"double", "dvec2", "dvec3", "dvec4"}, // Double types
        {"int", "ivec2", "ivec3", "ivec4"},    // Integer types
        {"uint", "uvec2", "uvec3", "uvec4"}    // Unsigned types
    };

    for (SVertexAttribInfo const* attrib = _VertexAttribs; attrib < &_VertexAttribs[_NumVertexAttribs]; attrib++, attribIndex++)
    {

        VERTEX_ATTRIB_COMPONENT typeOfComponent = attrib->TypeOfComponent();

        if (attrib->Mode == VAM_INTEGER && (typeOfComponent == COMPONENT_UBYTE || typeOfComponent == COMPONENT_USHORT || typeOfComponent == COMPONENT_UINT))
        {
            attribType = Types[3][attrib->NumComponents() - 1];
        }
        else
        {
            attribType = Types[attrib->Mode][attrib->NumComponents() - 1];
        }

        snprintf(location, sizeof(location), "%d", attrib->Location);

        s += "layout( location = ";
        s += location;
        s += " ) in ";
        s += attribType;
        s += " ";
        s += attrib->SemanticName;
        s += ";\n";
    }

    return s;
}

enum PRIMITIVE_TOPOLOGY : uint8_t
{
    PRIMITIVE_UNDEFINED          = 0,
    PRIMITIVE_POINTS             = 1,
    PRIMITIVE_LINES              = 2,
    PRIMITIVE_LINE_STRIP         = 3,
    PRIMITIVE_LINE_LOOP          = 4,
    PRIMITIVE_TRIANGLES          = 5,
    PRIMITIVE_TRIANGLE_STRIP     = 6,
    PRIMITIVE_TRIANGLE_FAN       = 7,
    PRIMITIVE_LINES_ADJ          = 8,
    PRIMITIVE_LINE_STRIP_ADJ     = 9,
    PRIMITIVE_TRIANGLES_ADJ      = 10,
    PRIMITIVE_TRIANGLE_STRIP_ADJ = 11,
    PRIMITIVE_PATCHES_1          = 12,
    PRIMITIVE_PATCHES_2          = PRIMITIVE_PATCHES_1 + 1,
    PRIMITIVE_PATCHES_3          = PRIMITIVE_PATCHES_1 + 2,
    PRIMITIVE_PATCHES_4          = PRIMITIVE_PATCHES_1 + 3,
    PRIMITIVE_PATCHES_5          = PRIMITIVE_PATCHES_1 + 4,
    PRIMITIVE_PATCHES_6          = PRIMITIVE_PATCHES_1 + 5,
    PRIMITIVE_PATCHES_7          = PRIMITIVE_PATCHES_1 + 6,
    PRIMITIVE_PATCHES_8          = PRIMITIVE_PATCHES_1 + 7,
    PRIMITIVE_PATCHES_9          = PRIMITIVE_PATCHES_1 + 8,
    PRIMITIVE_PATCHES_10         = PRIMITIVE_PATCHES_1 + 9,
    PRIMITIVE_PATCHES_11         = PRIMITIVE_PATCHES_1 + 10,
    PRIMITIVE_PATCHES_12         = PRIMITIVE_PATCHES_1 + 11,
    PRIMITIVE_PATCHES_13         = PRIMITIVE_PATCHES_1 + 12,
    PRIMITIVE_PATCHES_14         = PRIMITIVE_PATCHES_1 + 13,
    PRIMITIVE_PATCHES_15         = PRIMITIVE_PATCHES_1 + 14,
    PRIMITIVE_PATCHES_16         = PRIMITIVE_PATCHES_1 + 15,
    PRIMITIVE_PATCHES_17         = PRIMITIVE_PATCHES_1 + 16,
    PRIMITIVE_PATCHES_18         = PRIMITIVE_PATCHES_1 + 17,
    PRIMITIVE_PATCHES_19         = PRIMITIVE_PATCHES_1 + 18,
    PRIMITIVE_PATCHES_20         = PRIMITIVE_PATCHES_1 + 19,
    PRIMITIVE_PATCHES_21         = PRIMITIVE_PATCHES_1 + 20,
    PRIMITIVE_PATCHES_22         = PRIMITIVE_PATCHES_1 + 21,
    PRIMITIVE_PATCHES_23         = PRIMITIVE_PATCHES_1 + 22,
    PRIMITIVE_PATCHES_24         = PRIMITIVE_PATCHES_1 + 23,
    PRIMITIVE_PATCHES_25         = PRIMITIVE_PATCHES_1 + 24,
    PRIMITIVE_PATCHES_26         = PRIMITIVE_PATCHES_1 + 25,
    PRIMITIVE_PATCHES_27         = PRIMITIVE_PATCHES_1 + 26,
    PRIMITIVE_PATCHES_28         = PRIMITIVE_PATCHES_1 + 27,
    PRIMITIVE_PATCHES_29         = PRIMITIVE_PATCHES_1 + 28,
    PRIMITIVE_PATCHES_30         = PRIMITIVE_PATCHES_1 + 29,
    PRIMITIVE_PATCHES_31         = PRIMITIVE_PATCHES_1 + 30,
    PRIMITIVE_PATCHES_32         = PRIMITIVE_PATCHES_1 + 31
};

struct SPipelineInputAssemblyInfo
{
    PRIMITIVE_TOPOLOGY Topology = PRIMITIVE_TRIANGLES;
};

struct SPipelineDesc
{
    SPipelineInputAssemblyInfo IA;
    SBlendingStateInfo         BS;
    SRasterizerStateInfo       RS;
    SDepthStencilStateInfo     DSS;
    SPipelineResourceLayout    ResourceLayout;
    TRef<IShaderModule>        pVS;
    TRef<IShaderModule>        pTCS;
    TRef<IShaderModule>        pTES;
    TRef<IShaderModule>        pGS;
    TRef<IShaderModule>        pFS;
    TRef<IShaderModule>        pCS;
    uint32_t                   NumVertexBindings = 0;
    SVertexBindingInfo const*  pVertexBindings = nullptr;
    uint32_t                   NumVertexAttribs = 0;
    SVertexAttribInfo const*   pVertexAttribs = nullptr;
};

class IPipeline : public IDeviceObject
{
public:
    static constexpr DEVICE_OBJECT_PROXY_TYPE PROXY_TYPE = DEVICE_OBJECT_TYPE_PIPELINE;

    IPipeline(IDevice* pDevice) :
        IDeviceObject(pDevice, PROXY_TYPE)
    {}
};

} // namespace RenderCore
