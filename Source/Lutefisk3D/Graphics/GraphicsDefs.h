//
// Copyright (c) 2008-2016 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include "Lutefisk3D/Math/StringHash.h"
#include "Lutefisk3D/Container/FlagSet.h"

namespace Urho3D
{

class Vector3;

/// Primitive type.
enum PrimitiveType : unsigned
{
    TRIANGLE_LIST = 0,
    LINE_LIST,
    POINT_LIST,
    TRIANGLE_STRIP,
    LINE_STRIP,
    TRIANGLE_FAN
};

/// %Geometry type for vertex shader geometry variations.
enum GeometryType : unsigned
{
    GEOM_STATIC = 0,
    GEOM_SKINNED = 1,
    GEOM_INSTANCED = 2,
    GEOM_BILLBOARD = 3,
    GEOM_DIRBILLBOARD = 4,
    GEOM_TRAIL_FACE_CAMERA = 5,
    GEOM_TRAIL_BONE = 6,
    MAX_GEOMETRYTYPES = 7,
    // This is not a real geometry type for VS, but used to mark objects that do not desire to be instanced
    GEOM_STATIC_NOINSTANCING = 7,
};

/// Blending mode.
enum BlendMode : unsigned
{
    BLEND_REPLACE = 0,
    BLEND_ADD,
    BLEND_MULTIPLY,
    BLEND_ALPHA,
    BLEND_ADDALPHA,
    BLEND_PREMULALPHA,
    BLEND_INVDESTALPHA,
    BLEND_SUBTRACT,
    BLEND_SUBTRACTALPHA,
    BLEND_ZEROINVSRC, // SRC_ZERO,ONE_MINUS_SRC_COLOR
    MAX_BLENDMODES
};

/// Depth or stencil compare mode.
enum CompareMode : unsigned
{
    CMP_ALWAYS = 0,
    CMP_EQUAL,
    CMP_NOTEQUAL,
    CMP_LESS,
    CMP_LESSEQUAL,
    CMP_GREATER,
    CMP_GREATEREQUAL,
    MAX_COMPAREMODES
};

//TODO: make the common case = 0, and move CULL_NONE to the end of the enum
/// Culling mode.
enum CullMode : unsigned
{
    CULL_NONE = 0,
    CULL_CCW,
    CULL_CW,
    MAX_CULLMODES
};

/// Fill mode.
enum FillMode : unsigned
{
    FILL_SOLID = 0,
    FILL_WIREFRAME,
    FILL_POINT
};

/// Stencil operation.
enum StencilOp
{
    OP_KEEP = 0,
    OP_ZERO,
    OP_REF,
    OP_INCR,
    OP_DECR
};

/// Vertex/index buffer lock state.
enum LockState
{
    LOCK_NONE = 0,
    LOCK_HARDWARE,
    LOCK_SHADOW,
    LOCK_SCRATCH
};

/// Hardcoded legacy vertex elements.
enum LegacyVertexElement
{
    ELEMENT_POSITION = 0,
    ELEMENT_NORMAL,
    ELEMENT_COLOR,
    ELEMENT_TEXCOORD1,
    ELEMENT_TEXCOORD2,
    ELEMENT_CUBETEXCOORD1,
    ELEMENT_CUBETEXCOORD2,
    ELEMENT_TANGENT,
    ELEMENT_BLENDWEIGHTS,
    ELEMENT_BLENDINDICES,
    ELEMENT_INSTANCEMATRIX1,
    ELEMENT_INSTANCEMATRIX2,
    ELEMENT_INSTANCEMATRIX3,
    // Custom 32-bit integer object index. Due to API limitations, not supported on D3D9
    ELEMENT_OBJECTINDEX,
    MAX_LEGACY_VERTEX_ELEMENTS
};
/// Arbitrary vertex declaration element datatypes.
enum VertexElementType
{
    TYPE_INT = 0,
    TYPE_FLOAT,
    TYPE_VECTOR2,
    TYPE_VECTOR3,
    TYPE_VECTOR4,
    TYPE_UBYTE4,
    TYPE_UBYTE4_NORM,
    MAX_VERTEX_ELEMENT_TYPES
};

/// Arbitrary vertex declaration element semantics.
enum VertexElementSemantic
{
    SEM_POSITION = 0,
    SEM_NORMAL,
    SEM_BINORMAL,
    SEM_TANGENT,
    SEM_TEXCOORD,
    SEM_COLOR,
    SEM_BLENDWEIGHTS,
    SEM_BLENDINDICES,
    SEM_OBJECTINDEX,
    MAX_VERTEX_ELEMENT_SEMANTICS
};

/// Vertex element description for arbitrary vertex declarations.
struct LUTEFISK3D_EXPORT VertexElement
{
    /// Default-construct.
    VertexElement() noexcept :
        type_(TYPE_VECTOR3),
        semantic_(SEM_POSITION),
        index_(0),
        perInstance_(false),
        offset_(0)
    {
    }

    /// Construct with type, semantic, index and whether is per-instance data.
    VertexElement(VertexElementType type, VertexElementSemantic semantic, unsigned char index = 0, bool perInstance = false) noexcept :
        type_(type),
        semantic_(semantic),
        index_(index),
        perInstance_(perInstance),
        offset_(0)
    {
    }
    /// Test for equality with another vertex element. Offset is intentionally not compared, as it's relevant only when an element exists within a vertex buffer.
    bool operator ==(const VertexElement& rhs) const { return type_ == rhs.type_ && semantic_ == rhs.semantic_ && index_ == rhs.index_ && perInstance_ == rhs.perInstance_; }

    /// Test for inequality with another vertex element.
    bool operator !=(const VertexElement& rhs) const { return !(*this == rhs); }

    /// Data type of element.
    VertexElementType type_;
    /// Semantic of element.
    VertexElementSemantic semantic_;
    /// Semantic index of element, for example multi-texcoords.
    unsigned char index_;
    /// Per-instance flag.
    bool perInstance_;
    /// Offset of element from vertex start. Filled by VertexBuffer once the vertex declaration is built.
    unsigned offset_;
};
/// Sizes of vertex element types.
extern LUTEFISK3D_EXPORT const unsigned ELEMENT_TYPESIZES[];

/// Vertex element definitions for the legacy elements.
extern LUTEFISK3D_EXPORT const VertexElement LEGACY_VERTEXELEMENTS[];

/// Texture filtering mode.
enum TextureFilterMode : unsigned
{
    FILTER_NEAREST = 0,
    FILTER_BILINEAR,
    FILTER_TRILINEAR,
    FILTER_ANISOTROPIC,
    FILTER_NEAREST_ANISOTROPIC,
    FILTER_DEFAULT,
    MAX_FILTERMODES
};

/// Texture addressing mode.
enum TextureAddressMode
{
    ADDRESS_WRAP = 0,
    ADDRESS_MIRROR,
    ADDRESS_CLAMP,
    ADDRESS_BORDER,
    MAX_ADDRESSMODES
};

/// Texture coordinates.
enum TextureCoordinate
{
    COORD_U = 0,
    COORD_V,
    COORD_W,
    MAX_COORDS
};

/// Texture usage types.
enum TextureUsage : uint8_t
{
    TEXTURE_STATIC = 0,
    TEXTURE_DYNAMIC,
    TEXTURE_RENDERTARGET,
    TEXTURE_DEPTHSTENCIL
};

/// Cube map faces.
enum CubeMapFace
{
    FACE_POSITIVE_X = 0,
    FACE_NEGATIVE_X,
    FACE_POSITIVE_Y,
    FACE_NEGATIVE_Y,
    FACE_POSITIVE_Z,
    FACE_NEGATIVE_Z,
    MAX_CUBEMAP_FACES
};

/// Cubemap single image layout modes.
enum CubeMapLayout
{
    CML_HORIZONTAL = 0,
    CML_HORIZONTALNVIDIA,
    CML_HORIZONTALCROSS,
    CML_VERTICALCROSS,
    CML_BLENDER
};

/// Update mode for render surface viewports.
enum RenderSurfaceUpdateMode : uint8_t
{
    SURFACE_MANUALUPDATE = 0,
    SURFACE_UPDATEVISIBLE,
    SURFACE_UPDATEALWAYS
};

/// Shader types.
enum ShaderType
{
    VS = 0,
    PS,
};

/// Shader parameter groups for determining need to update. On APIs that support constant buffers, these correspond to different constant buffers.
enum ShaderParameterGroup
{
    SP_FRAME = 0,
    SP_CAMERA,
    SP_ZONE,
    SP_LIGHT,
    SP_MATERIAL,
    SP_OBJECT,
    SP_CUSTOM,
    MAX_SHADER_PARAMETER_GROUPS
};

/// Texture units.
enum TextureUnit : unsigned
{
    TU_DIFFUSE = 0,
    TU_ALBEDOBUFFER = 0,
    TU_NORMAL = 1,
    TU_NORMALBUFFER = 1,
    TU_SPECULAR = 2,
    TU_EMISSIVE = 3,
    TU_ENVIRONMENT = 4,
    TU_VOLUMEMAP = 5,
    TU_CUSTOM1 = 6,
    TU_CUSTOM2 = 7,
    TU_LIGHTRAMP = 8,
    TU_LIGHTSHAPE = 9,
    TU_SHADOWMAP = 10,
    TU_FACESELECT = 11,
    TU_INDIRECTION = 12,
    TU_DEPTHBUFFER = 13,
    TU_LIGHTBUFFER = 14,
    TU_ZONE = 15,
    MAX_MATERIAL_TEXTURE_UNITS = 8,
    MAX_TEXTURE_UNITS = 16
};

/// Billboard camera facing modes.
enum FaceCameraMode : unsigned
{
    FC_NONE = 0,
    FC_ROTATE_XYZ,
    FC_ROTATE_Y,
    FC_LOOKAT_XYZ,
    FC_LOOKAT_Y,
    FC_LOOKAT_MIXED,
    FC_DIRECTION,
};
/// Shadow type.
enum ShadowQuality : unsigned
{
    SHADOWQUALITY_SIMPLE_16BIT = 0,
    SHADOWQUALITY_SIMPLE_24BIT,
    SHADOWQUALITY_PCF_16BIT,
    SHADOWQUALITY_PCF_24BIT,
    SHADOWQUALITY_VSM,
    SHADOWQUALITY_BLUR_VSM
};
// Light constants
static constexpr const unsigned MAX_LIGHT_SPLITS = 6;

// Inbuilt shader parameters.
extern LUTEFISK3D_EXPORT const StringHash VSP_AMBIENTSTARTCOLOR;
extern LUTEFISK3D_EXPORT const StringHash VSP_AMBIENTENDCOLOR;
extern LUTEFISK3D_EXPORT const StringHash VSP_BILLBOARDROT;
extern LUTEFISK3D_EXPORT const StringHash VSP_CAMERAPOS;
extern LUTEFISK3D_EXPORT const StringHash VSP_CLIPPLANE;
extern LUTEFISK3D_EXPORT const StringHash VSP_NEARCLIP;
extern LUTEFISK3D_EXPORT const StringHash VSP_FARCLIP;
extern LUTEFISK3D_EXPORT const StringHash VSP_DEPTHMODE;
extern LUTEFISK3D_EXPORT const StringHash VSP_DELTATIME;
extern LUTEFISK3D_EXPORT const StringHash VSP_ELAPSEDTIME;
extern LUTEFISK3D_EXPORT const StringHash VSP_FRUSTUMSIZE;
extern LUTEFISK3D_EXPORT const StringHash VSP_GBUFFEROFFSETS;
extern LUTEFISK3D_EXPORT const StringHash VSP_LIGHTDIR;
extern LUTEFISK3D_EXPORT const StringHash VSP_LIGHTPOS;
extern LUTEFISK3D_EXPORT const StringHash VSP_NORMALOFFSETSCALE;
extern LUTEFISK3D_EXPORT const StringHash VSP_MODEL;
extern LUTEFISK3D_EXPORT const StringHash VSP_VIEW;
extern LUTEFISK3D_EXPORT const StringHash VSP_VIEWINV;
extern LUTEFISK3D_EXPORT const StringHash VSP_VIEWPROJ;
extern LUTEFISK3D_EXPORT const StringHash VSP_UOFFSET;
extern LUTEFISK3D_EXPORT const StringHash VSP_VOFFSET;
extern LUTEFISK3D_EXPORT const StringHash VSP_ZONE;
extern LUTEFISK3D_EXPORT const StringHash VSP_LIGHTMATRICES;
extern LUTEFISK3D_EXPORT const StringHash VSP_SKINMATRICES;
extern LUTEFISK3D_EXPORT const StringHash VSP_VERTEXLIGHTS;
extern LUTEFISK3D_EXPORT const StringHash PSP_AMBIENTCOLOR;
extern LUTEFISK3D_EXPORT const StringHash PSP_CAMERAPOS;
extern LUTEFISK3D_EXPORT const StringHash PSP_DELTATIME;
extern LUTEFISK3D_EXPORT const StringHash PSP_DEPTHRECONSTRUCT;
extern LUTEFISK3D_EXPORT const StringHash PSP_ELAPSEDTIME;
extern LUTEFISK3D_EXPORT const StringHash PSP_FOGCOLOR;
extern LUTEFISK3D_EXPORT const StringHash PSP_FOGPARAMS;
extern LUTEFISK3D_EXPORT const StringHash PSP_GBUFFERINVSIZE;
extern LUTEFISK3D_EXPORT const StringHash PSP_LIGHTCOLOR;
extern LUTEFISK3D_EXPORT const StringHash PSP_LIGHTDIR;
extern LUTEFISK3D_EXPORT const StringHash PSP_LIGHTPOS;
extern LUTEFISK3D_EXPORT const StringHash PSP_NORMALOFFSETSCALE;
extern LUTEFISK3D_EXPORT const StringHash PSP_MATDIFFCOLOR;
extern LUTEFISK3D_EXPORT const StringHash PSP_MATEMISSIVECOLOR;
extern LUTEFISK3D_EXPORT const StringHash PSP_MATENVMAPCOLOR;
extern LUTEFISK3D_EXPORT const StringHash PSP_MATSPECCOLOR;
extern LUTEFISK3D_EXPORT const StringHash PSP_NEARCLIP;
extern LUTEFISK3D_EXPORT const StringHash PSP_FARCLIP;
extern LUTEFISK3D_EXPORT const StringHash PSP_SHADOWCUBEADJUST;
extern LUTEFISK3D_EXPORT const StringHash PSP_SHADOWDEPTHFADE;
extern LUTEFISK3D_EXPORT const StringHash PSP_SHADOWINTENSITY;
extern LUTEFISK3D_EXPORT const StringHash PSP_SHADOWMAPINVSIZE;
extern LUTEFISK3D_EXPORT const StringHash PSP_SHADOWSPLITS;
extern LUTEFISK3D_EXPORT const StringHash PSP_LIGHTMATRICES;
extern LUTEFISK3D_EXPORT const StringHash PSP_VSMSHADOWPARAMS;
extern LUTEFISK3D_EXPORT const StringHash PSP_ROUGHNESS;
extern LUTEFISK3D_EXPORT const StringHash PSP_METALLIC;
extern LUTEFISK3D_EXPORT const StringHash PSP_LIGHTRAD;
extern LUTEFISK3D_EXPORT const StringHash PSP_LIGHTLENGTH;
extern LUTEFISK3D_EXPORT const StringHash PSP_ZONEMIN;
extern LUTEFISK3D_EXPORT const StringHash PSP_ZONEMAX;
// Scale calculation from bounding box diagonal.
extern LUTEFISK3D_EXPORT const Vector3 DOT_SCALE;

enum eQuality : int {
    QUALITY_LOW = 0,
    QUALITY_MEDIUM = 1,
    QUALITY_HIGH = 2,
    QUALITY_MAX = 15
};
enum ClearTarget : unsigned
{
    CLEAR_COLOR = 0x1,
    CLEAR_DEPTH = 0x2,
    CLEAR_STENCIL = 0x4,
};
URHO3D_FLAGSET(ClearTarget, ClearTargetFlags);

// Legacy vertex element bitmasks.
enum VertexMask : unsigned
{
    MASK_NONE = 0x0,
    MASK_POSITION = 0x1,
    MASK_NORMAL = 0x2,
    MASK_COLOR = 0x4,
    MASK_TEXCOORD1 = 0x8,
    MASK_TEXCOORD2 = 0x10,
    MASK_CUBETEXCOORD1 = 0x20,
    MASK_CUBETEXCOORD2 = 0x40,
    MASK_TANGENT = 0x80,
    MASK_BLENDWEIGHTS = 0x100,
    MASK_BLENDINDICES = 0x200,
    MASK_INSTANCEMATRIX1 = 0x400,
    MASK_INSTANCEMATRIX2 = 0x800,
    MASK_INSTANCEMATRIX3 = 0x1000,
    MASK_OBJECTINDEX = 0x2000,
};
URHO3D_FLAGSET(VertexMask, VertexMaskFlags);

static const int MAX_RENDERTARGETS = 4;
static const int MAX_VERTEX_STREAMS = 4;
static const int MAX_CONSTANT_REGISTERS = 256;

static const int BITS_PER_COMPONENT = 8;

}
