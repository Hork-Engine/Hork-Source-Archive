/*

Angie Engine Source Code

MIT License

Copyright (C) 2017-2020 Alexander Samusev.

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

#include "OpenGL45Common.h"

#include "OpenGL45IrradianceGenerator.h"
#include "OpenGL45EnvProbeGenerator.h"
#include "OpenGL45CubemapGenerator.h"

#include <Core/Public/Color.h>
#include <Core/Public/Image.h>
#include <Core/Public/CriticalError.h>
#include <Core/Public/Logger.h>

ARuntimeVariable RVDebugRenderMode( _CTS( "DebugRenderMode" ), _CTS( "0" ), VAR_CHEAT );
ARuntimeVariable RVPostprocessBloomScale( _CTS( "PostprocessBloomScale" ), _CTS( "1" ) );
ARuntimeVariable RVPostprocessBloom( _CTS( "PostprocessBloom" ), _CTS( "1" ) );
ARuntimeVariable RVPostprocessBloomParam0( _CTS( "PostprocessBloomParam0" ), _CTS( "0.5" ) );
ARuntimeVariable RVPostprocessBloomParam1( _CTS( "PostprocessBloomParam1" ), _CTS( "0.3" ) );
ARuntimeVariable RVPostprocessBloomParam2( _CTS( "PostprocessBloomParam2" ), _CTS( "0.04" ) );
ARuntimeVariable RVPostprocessBloomParam3( _CTS( "PostprocessBloomParam3" ), _CTS( "0.01" ) );
//ARuntimeVariable RVPostprocessToneExposure( _CTS( "PostprocessToneExposure" ), _CTS( "0.05" ) );
ARuntimeVariable RVPostprocessToneExposure( _CTS( "PostprocessToneExposure" ), _CTS( "0.4" ) );
ARuntimeVariable RVBrightness( _CTS( "Brightness" ), _CTS( "1" ) );
extern ARuntimeVariable RVFxaa;

namespace OpenGL45 {

GHI::Device          GDevice;
GHI::State           GState;
GHI::CommandBuffer   Cmd;
SRenderFrame *       GFrameData;
SRenderView *        GRenderView;
ARenderArea          GRenderViewArea;
AShaderSources       GShaderSources;
AFrameResources      GFrameResources;

GHI::TextureResolution2D GetFrameResoultion()
{
    return GHI::TextureResolution2D( GFrameData->AllocSurfaceWidth, GFrameData->AllocSurfaceHeight );
}

void DrawSAQ( GHI::Pipeline * Pipeline ) {
    const GHI::DrawCmd drawCmd = { 4, 1, 0, 0 };
    Cmd.BindPipeline( Pipeline );
    Cmd.BindVertexBuffer( 0, &GFrameResources.Saq, 0 );
    Cmd.BindIndexBuffer( NULL, GHI::INDEX_TYPE_UINT16, 0 );
    Cmd.Draw( &drawCmd );
}

void BindTextures( SMaterialFrameData * _MaterialInstance ) {
    AN_ASSERT( _MaterialInstance );

    ATextureGPU ** texture = _MaterialInstance->Textures;

    int n = _MaterialInstance->NumTextures;
    if ( n > _MaterialInstance->Material->NumSamplers ) {
        n = _MaterialInstance->Material->NumSamplers;
    }

    for ( int t = 0 ; t < n ; t++, texture++ ) {
        if ( *texture ) {
            ATextureGPU * texProxy = *texture;
            GFrameResources.TextureBindings[t].pTexture = GPUTextureHandle( texProxy );
        } else {
            GFrameResources.TextureBindings[t].pTexture = nullptr;
        }
    }

    //for (int t = n ; t<MAX_MATERIAL_TEXTURES ; t++ ) {
    //    TextureBindings[t].pTexture = nullptr;
    //}
}

void BindVertexAndIndexBuffers( SRenderInstance const * _Instance ) {
    GHI::Buffer * pVertexBuffer = GPUBufferHandle( _Instance->VertexBuffer );
    GHI::Buffer * pIndexBuffer = GPUBufferHandle( _Instance->IndexBuffer );

    AN_ASSERT( pVertexBuffer );
    AN_ASSERT( pIndexBuffer );

    Cmd.BindVertexBuffer( 0, pVertexBuffer, _Instance->VertexBufferOffset );
    Cmd.BindIndexBuffer( pIndexBuffer, GHI::INDEX_TYPE_UINT32, _Instance->IndexBufferOffset );
}

void BindVertexAndIndexBuffers( SShadowRenderInstance const * _Instance ) {
    GHI::Buffer * pVertexBuffer = GPUBufferHandle( _Instance->VertexBuffer );
    GHI::Buffer * pIndexBuffer = GPUBufferHandle( _Instance->IndexBuffer );

    AN_ASSERT( pVertexBuffer );
    AN_ASSERT( pIndexBuffer );

    Cmd.BindVertexBuffer( 0, pVertexBuffer, _Instance->VertexBufferOffset );
    Cmd.BindIndexBuffer( pIndexBuffer, GHI::INDEX_TYPE_UINT32, _Instance->IndexBufferOffset );
}

void BindSkeleton( size_t _Offset, size_t _Size ) {
    GFrameResources.SkeletonBufferBinding->BindingOffset = _Offset;
    GFrameResources.SkeletonBufferBinding->BindingSize = _Size;
}

void SetInstanceUniforms( SRenderInstance const * Instance, int _Index ) {
    size_t offset = GFrameResources.ConstantBuffer->Allocate( sizeof( SInstanceUniformBuffer ) );

    SInstanceUniformBuffer * pUniformBuf = reinterpret_cast< SInstanceUniformBuffer * >(GFrameResources.ConstantBuffer->GetMappedMemory() + offset);

    Core::Memcpy( &pUniformBuf->TransformMatrix, &Instance->Matrix, sizeof( pUniformBuf->TransformMatrix ) );
    StoreFloat3x3AsFloat3x4Transposed( Instance->ModelNormalToViewSpace, pUniformBuf->ModelNormalToViewSpace );
    Core::Memcpy( &pUniformBuf->LightmapOffset, &Instance->LightmapOffset, sizeof( pUniformBuf->LightmapOffset ) );
    Core::Memcpy( &pUniformBuf->uaddr_0, Instance->MaterialInstance->UniformVectors, sizeof( Float4 )*Instance->MaterialInstance->NumUniformVectors );

    GFrameResources.InstanceUniformBufferBinding->pBuffer = GFrameResources.ConstantBuffer->GetBuffer();
    GFrameResources.InstanceUniformBufferBinding->BindingOffset = offset;
    GFrameResources.InstanceUniformBufferBinding->BindingSize = sizeof( SInstanceUniformBuffer );
}

void SetShadowInstanceUniforms( SShadowRenderInstance const * Instance, int _Index ) {
    size_t offset = GFrameResources.ConstantBuffer->Allocate( sizeof( SShadowInstanceUniformBuffer ) );

    SShadowInstanceUniformBuffer * pUniformBuf = reinterpret_cast< SShadowInstanceUniformBuffer * >(GFrameResources.ConstantBuffer->GetMappedMemory() + offset);

    StoreFloat3x4AsFloat4x4Transposed( Instance->WorldTransformMatrix, pUniformBuf->TransformMatrix );

    if ( Instance->MaterialInstance ) {
        Core::Memcpy( &pUniformBuf->uaddr_0, Instance->MaterialInstance->UniformVectors, sizeof( Float4 )*Instance->MaterialInstance->NumUniformVectors );
    }

    GFrameResources.InstanceUniformBufferBinding->pBuffer = GFrameResources.ConstantBuffer->GetBuffer();
    GFrameResources.InstanceUniformBufferBinding->BindingOffset = offset;
    GFrameResources.InstanceUniformBufferBinding->BindingSize = sizeof( SShadowInstanceUniformBuffer );
}

void CreateFullscreenQuadPipeline( GHI::Pipeline & Pipe, const char * VertexShader, const char * FragmentShader, GHI::BLENDING_PRESET BlendingPreset,
                                   GHI::ShaderModule * pVertexShaderModule, GHI::ShaderModule * pFragmentShaderModule ) {
    using namespace GHI;

    RasterizerStateInfo rsd;
    rsd.SetDefaults();
    rsd.CullMode = POLYGON_CULL_FRONT;
    rsd.bScissorEnable = false;

    BlendingStateInfo bsd;
    bsd.SetDefaults();

    if ( BlendingPreset != BLENDING_NO_BLEND ) {
        bsd.RenderTargetSlots[0].SetBlendingPreset( BlendingPreset );
    }

    DepthStencilStateInfo dssd;
    dssd.SetDefaults();

    dssd.bDepthEnable = false;
    dssd.DepthWriteMask = DEPTH_WRITE_DISABLE;

    static const VertexAttribInfo vertexAttribs[] = {
        {
            "InPosition",
            0,              // location
            0,              // buffer input slot
            VAT_FLOAT2,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            0
        }
    };

    AString vertexAttribsShaderString = ShaderStringForVertexAttribs< AString >( vertexAttribs, AN_ARRAY_SIZE( vertexAttribs ) );

    ShaderModule vertexShaderModule, fragmentShaderModule;

    if ( !pVertexShaderModule ) {
        pVertexShaderModule = &vertexShaderModule;
    }

    if ( !pFragmentShaderModule ) {
        pFragmentShaderModule = &fragmentShaderModule;
    }

    AString vertexSourceCode = LoadShader( VertexShader );
    AString fragmentSourceCode = LoadShader( FragmentShader );

    GShaderSources.Clear();
    GShaderSources.Add( vertexAttribsShaderString.CStr() );
    GShaderSources.Add( vertexSourceCode.CStr() );
    GShaderSources.Build( VERTEX_SHADER, pVertexShaderModule );

    GShaderSources.Clear();
    GShaderSources.Add( fragmentSourceCode.CStr() );
    GShaderSources.Build( FRAGMENT_SHADER, pFragmentShaderModule );

    PipelineCreateInfo pipelineCI = {};

    PipelineInputAssemblyInfo inputAssembly = {};
    inputAssembly.Topology = PRIMITIVE_TRIANGLE_STRIP;
    inputAssembly.bPrimitiveRestart = false;

    pipelineCI.pInputAssembly = &inputAssembly;
    pipelineCI.pRasterizer = &rsd;
    pipelineCI.pDepthStencil = &dssd;

    ShaderStageInfo vs = {};
    vs.Stage = SHADER_STAGE_VERTEX_BIT;
    vs.pModule = pVertexShaderModule;

    ShaderStageInfo fs = {};
    fs.Stage = SHADER_STAGE_FRAGMENT_BIT;
    fs.pModule = pFragmentShaderModule;

    ShaderStageInfo stages[] = { vs, fs };

    pipelineCI.NumStages = AN_ARRAY_SIZE( stages );
    pipelineCI.pStages = stages;

    VertexBindingInfo vertexBinding[1] = {};

    vertexBinding[0].InputSlot = 0;
    vertexBinding[0].Stride = sizeof( Float2 );
    vertexBinding[0].InputRate = INPUT_RATE_PER_VERTEX;

    pipelineCI.NumVertexBindings = AN_ARRAY_SIZE( vertexBinding );
    pipelineCI.pVertexBindings = vertexBinding;

    pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
    pipelineCI.pVertexAttribs = vertexAttribs;

    pipelineCI.pBlending = &bsd;
    Pipe.Initialize( pipelineCI );
}

void CreateFullscreenQuadPipelineGS( GHI::Pipeline & Pipe, const char * VertexShader, const char * FragmentShader, const char * GeometryShader, GHI::BLENDING_PRESET BlendingPreset,
                                     GHI::ShaderModule * pVertexShaderModule, GHI::ShaderModule * pFragmentShaderModule, GHI::ShaderModule * pGeometryShaderModule ) {
    using namespace GHI;

    RasterizerStateInfo rsd;
    rsd.SetDefaults();
    rsd.CullMode = POLYGON_CULL_FRONT;
    rsd.bScissorEnable = false;

    BlendingStateInfo bsd;
    bsd.SetDefaults();

    if ( BlendingPreset != BLENDING_NO_BLEND ) {
        bsd.RenderTargetSlots[0].SetBlendingPreset( BlendingPreset );
    }

    DepthStencilStateInfo dssd;
    dssd.SetDefaults();

    dssd.bDepthEnable = false;
    dssd.DepthWriteMask = DEPTH_WRITE_DISABLE;

    static const VertexAttribInfo vertexAttribs[] = {
        {
            "InPosition",
            0,              // location
            0,              // buffer input slot
            VAT_FLOAT2,
            VAM_FLOAT,
            0,              // InstanceDataStepRate
            0
        }
    };

    AString vertexAttribsShaderString = ShaderStringForVertexAttribs< AString >( vertexAttribs, AN_ARRAY_SIZE( vertexAttribs ) );

    ShaderModule vertexShaderModule, fragmentShaderModule, geometryShaderModule;

    if ( !pVertexShaderModule ) {
        pVertexShaderModule = &vertexShaderModule;
    }

    if ( !pFragmentShaderModule ) {
        pFragmentShaderModule = &fragmentShaderModule;
    }

    if ( !pGeometryShaderModule ) {
        pGeometryShaderModule = &geometryShaderModule;
    }

    AString vertexSourceCode = LoadShader( VertexShader );
    AString fragmentSourceCode = LoadShader( FragmentShader );
    AString geometrySourceCode = LoadShader( GeometryShader );

    GShaderSources.Clear();
    GShaderSources.Add( vertexAttribsShaderString.CStr() );
    GShaderSources.Add( vertexSourceCode.CStr() );
    GShaderSources.Build( VERTEX_SHADER, pVertexShaderModule );

    GShaderSources.Clear();
    GShaderSources.Add( fragmentSourceCode.CStr() );
    GShaderSources.Build( FRAGMENT_SHADER, pFragmentShaderModule );

    GShaderSources.Clear();
    GShaderSources.Add( geometrySourceCode.CStr() );
    GShaderSources.Build( GEOMETRY_SHADER, pGeometryShaderModule );

    PipelineCreateInfo pipelineCI = {};

    PipelineInputAssemblyInfo inputAssembly = {};
    inputAssembly.Topology = PRIMITIVE_TRIANGLE_STRIP;
    inputAssembly.bPrimitiveRestart = false;

    pipelineCI.pInputAssembly = &inputAssembly;
    pipelineCI.pRasterizer = &rsd;
    pipelineCI.pDepthStencil = &dssd;

    ShaderStageInfo vs = {};
    vs.Stage = SHADER_STAGE_VERTEX_BIT;
    vs.pModule = pVertexShaderModule;

    ShaderStageInfo gs = {};
    gs.Stage = SHADER_STAGE_GEOMETRY_BIT;
    gs.pModule = pGeometryShaderModule;

    ShaderStageInfo fs = {};
    fs.Stage = SHADER_STAGE_FRAGMENT_BIT;
    fs.pModule = pFragmentShaderModule;

    ShaderStageInfo stages[] = { vs, gs, fs };

    pipelineCI.NumStages = AN_ARRAY_SIZE( stages );
    pipelineCI.pStages = stages;

    VertexBindingInfo vertexBinding[1] = {};

    vertexBinding[0].InputSlot = 0;
    vertexBinding[0].Stride = sizeof( Float2 );
    vertexBinding[0].InputRate = INPUT_RATE_PER_VERTEX;

    pipelineCI.NumVertexBindings = AN_ARRAY_SIZE( vertexBinding );
    pipelineCI.pVertexBindings = vertexBinding;

    pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
    pipelineCI.pVertexAttribs = vertexAttribs;

    pipelineCI.pBlending = &bsd;
    Pipe.Initialize( pipelineCI );
}

void SaveSnapshot( GHI::Texture & _Texture ) {

    const int w = _Texture.GetWidth();
    const int h = _Texture.GetHeight();
    const int numchannels = 3;
    const int size = w * h * numchannels;

    int hunkMark = GHunkMemory.SetHunkMark();

    byte * data = (byte *)GHunkMemory.Alloc( size );

#if 0
    _Texture.Read( 0, GHI::PIXEL_FORMAT_BYTE_RGB, size, 1, data );
#else
    float * fdata = (float *)GHunkMemory.Alloc( size*sizeof(float) );
    _Texture.Read( 0, GHI::PIXEL_FORMAT_FLOAT_RGB, size*sizeof(float), 1, fdata );
    // to sRGB
    for ( int i = 0 ; i < size ; i++ ) {
        data[i] = LinearToSRGB_UChar( fdata[i] );
    }
#endif

    FlipImageY( data, w, h, numchannels, w * numchannels );
    
    static int n = 0;
    AFileStream f;
    if ( f.OpenWrite( Core::Fmt( "snapshots/%d.png", n++ ) ) ) {
         WritePNG( f, w, h, numchannels, data, w*numchannels );
    }

    GHunkMemory.ClearToMark( hunkMark );
}

struct SIncludeCtx {
    /** Callback for file loading */
    bool ( *LoadFile )( const char * FileName, AString & Source );

    /** Root path for includes */
    const char * PathToIncludes;

    /** Predefined shaders */
    SMaterialShader const * Predefined;
};

// Modified version of stb_include.h v0.02 originally written by Sean Barrett and Michal Klos

struct SIncludeInfo
{
    int offset;
    int end;
    const char *filename;
    int len;
    int next_line_after;
    SIncludeInfo * next;
};

static void AddInclude( SIncludeInfo *& list, SIncludeInfo *& prev, int offset, int end, const char *filename, int len, int next_line ) {
    SIncludeInfo *z = (SIncludeInfo *)GZoneMemory.Alloc( sizeof( SIncludeInfo ) );
    z->offset = offset;
    z->end = end;
    z->filename = filename;
    z->len = len;
    z->next_line_after = next_line;
    z->next = NULL;
    if ( prev ) {
        prev->next = z;
        prev = z;
    } else {
        list = prev = z;
    }
}

static void FreeIncludes( SIncludeInfo * list ) {
    SIncludeInfo * next;
    for ( SIncludeInfo * includeInfo = list ; includeInfo ; includeInfo = next ) {
        next = includeInfo->next;
        GZoneMemory.Free( includeInfo );
    }
}

static AN_FORCEINLINE int IsSpace( int ch ) {
    return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
}

// find location of all #include
static SIncludeInfo * FindIncludes( const char * text ) {
    int line_count = 1;
    const char *s = text, *start;
    SIncludeInfo *list = NULL, *prev = NULL;
    while ( *s ) {
        // parse is always at start of line when we reach here
        start = s;
        while ( *s == ' ' || *s == '\t' ) {
            ++s;
        }
        if ( *s == '#' ) {
            ++s;
            while ( *s == ' ' || *s == '\t' )
                ++s;
            if ( !Core::StrcmpN( s, "include", 7 ) && IsSpace( s[7] ) ) {
                s += 7;
                while ( *s == ' ' || *s == '\t' )
                    ++s;
                if ( *s == '"' ) {
                    const char *t = ++s;
                    while ( *t != '"' && *t != '\n' && *t != '\r' && *t != 0 )
                        ++t;
                    if ( *t == '"' ) {
                        int len = t - s;
                        const char * filename = s;
                        s = t;
                        while ( *s != '\r' && *s != '\n' && *s != 0 )
                            ++s;
                        // s points to the newline, so s-start is everything except the newline
                        AddInclude( list, prev, start-text, s-text, filename, len, line_count+1 );
                    }
                }
            }
        }
        while ( *s != '\r' && *s != '\n' && *s != 0 )
            ++s;
        if ( *s == '\r' || *s == '\n' ) {
            s = s + (s[0] + s[1] == '\r' + '\n' ? 2 : 1);
        }
        ++line_count;
    }
    return list;
}

static void CleanComments( char * s ) {
start:
    while ( *s ) {
        if ( *s == '/' ) {
            if ( *(s+1) == '/' ) {
                *s++ = ' ';
                *s++ = ' ';
                while ( *s && *s != '\n' )
                    *s++ = ' ';
                continue;
            }
            if ( *(s+1) == '*' ) {
                *s++ = ' ';
                *s++ = ' ';
                while ( *s ) {
                    if ( *s == '*' && *(s+1) == '/' ) {
                        *s++ = ' ';
                        *s++ = ' ';
                        goto start;
                    }
                    if ( *s != '\n' ) {
                        *s++ = ' ';
                    } else {
                        s++;
                    }
                }
                // end of file inside comment
                return;
            }
        }
        s++;
    }
}

static bool LoadShaderWithInclude( SIncludeCtx * Ctx, const char * FileName, AString & Out );

static bool LoadShaderFromString( SIncludeCtx * Ctx, const char * FileName, AString const & Source, AString & Out ) {
    char temp[4096];
    SIncludeInfo * includeList = FindIncludes( Source.CStr() );
    size_t sourceOffset = 0;

    for ( SIncludeInfo * includeInfo = includeList ; includeInfo ; includeInfo = includeInfo->next ) {
        Out.ConcatN( &Source[sourceOffset], includeInfo->offset - sourceOffset );

        if ( Ctx->Predefined && includeInfo->filename[0] == '$' ) {
            // predefined source
            Out.Concat( "#line 1 \"" );
            Out.ConcatN( includeInfo->filename, includeInfo->len );
            Out.Concat( "\"\n" );

            SMaterialShader const * s;
            for ( s = Ctx->Predefined ; s ; s = s->Next ) {
                if ( !Core::StricmpN( s->SourceName, includeInfo->filename, includeInfo->len ) ) {
                    break;
                }
            }

            if ( !s || !LoadShaderFromString( Ctx, FileName, s->Code, Out ) ) {
                FreeIncludes( includeList );
                return false;
            }
        } else {
            Out.Concat( "#line 1 \"" );
            Out.Concat( Ctx->PathToIncludes );
            Out.ConcatN( includeInfo->filename, includeInfo->len );
            Out.Concat( "\"\n" );

            Core::Strcpy( temp, sizeof( temp ), Ctx->PathToIncludes );
            Core::StrcatN( temp, sizeof( temp ), includeInfo->filename, includeInfo->len );
            if ( !LoadShaderWithInclude( Ctx, temp, Out ) ) {
                FreeIncludes( includeList );
                return false;
            }
        }        

        Core::Sprintf( temp, sizeof( temp ), "\n#line %d \"%s\"", includeInfo->next_line_after, FileName ? FileName : "source-file" );
        Out.Concat( temp );

        sourceOffset = includeInfo->end;
    }

    Out.ConcatN( &Source[sourceOffset], Source.Length() - sourceOffset + 1 );
    FreeIncludes( includeList );

    return true;
}

static bool LoadShaderWithInclude( SIncludeCtx * Ctx, const char * FileName, AString & Out ) {
    AString source;

    if ( !Ctx->LoadFile( FileName, source ) ) {
        GLogger.Printf( "Couldn't load %s\n", FileName );
        return false;
    }

    CleanComments( source.ToPtr() );

    return LoadShaderFromString( Ctx, FileName, source, Out );
}

static bool GetShaderSource( const char * FileName, AString & Source ) {
    AFileStream f;
    if ( !f.OpenRead( FileName ) ) {
        return false;
    }
    Source.FromFile( f );
    return true;
}

AString LoadShader( const char * FileName, SMaterialShader const * Predefined ) {
    AString path = __FILE__;
    path.StripFilename();
    path.FixPath();
    path += "/Shaders/";

    SIncludeCtx ctx;
    ctx.LoadFile = GetShaderSource;
    ctx.PathToIncludes = path.CStr();
    ctx.Predefined = Predefined;

    AString result;
    result.Concat( Core::Fmt( "#line 1 \"%s\"\n", FileName ) );

    if ( !LoadShaderWithInclude( &ctx, (path + FileName).CStr(), result ) ) {
        CriticalError( "LoadShader: failed to open %s\n", FileName );
    }

    return result;
}

AString LoadShaderFromString( const char * FileName, const char * Source, SMaterialShader const * Predefined ) {
    AString path = __FILE__;
    path.StripFilename();
    path.FixPath();
    path += "/Shaders/";

    SIncludeCtx ctx;
    ctx.LoadFile = GetShaderSource;
    ctx.PathToIncludes = path.CStr();
    ctx.Predefined = Predefined;

    AString result;
    result.Concat( Core::Fmt( "#line 1 \"%s\"\n", FileName ) );

    AString source = Source;

    CleanComments( source.ToPtr() );

    if ( !LoadShaderFromString( &ctx, (path + FileName).CStr(), source, result ) ) {
        CriticalError( "LoadShader: failed to open %s\n", FileName );
    }

    return result;
}

void AFrameResources::Initialize() {
    ConstantBuffer = std::make_unique< ACircularBuffer >( 2 * 1024 * 1024 ); // 2MB
    FrameConstantBuffer = std::make_unique< AFrameConstantBuffer >( 2 * 1024 * 1024 ); // 2MB

    {
        GHI::TextureStorageCreateInfo createInfo = {};
        createInfo.Type = GHI::TEXTURE_3D;
        createInfo.InternalFormat = GHI::INTERNAL_PIXEL_FORMAT_RG32UI;
        createInfo.Resolution.Tex3D.Width = MAX_FRUSTUM_CLUSTERS_X;
        createInfo.Resolution.Tex3D.Height = MAX_FRUSTUM_CLUSTERS_Y;
        createInfo.Resolution.Tex3D.Depth = MAX_FRUSTUM_CLUSTERS_Z;
        createInfo.NumLods = 1;
        ClusterLookup.InitializeStorage( createInfo );
    }

    {
        // FIXME: Use SSBO?
        GHI::BufferCreateInfo bufferCI = {};
        bufferCI.bImmutableStorage = true;
        bufferCI.ImmutableStorageFlags = GHI::IMMUTABLE_DYNAMIC_STORAGE;
        bufferCI.SizeInBytes = sizeof( SFrameLightData::ItemBuffer );
        ClusterItemBuffer.Initialize( bufferCI );
        ClusterItemTBO.InitializeTextureBuffer( GHI::BUFFER_DATA_UINT1, ClusterItemBuffer );
    }

    {
        constexpr Float2 saqVertices[4] = {
            { Float2( -1.0f,  1.0f ) },
            { Float2( 1.0f,  1.0f ) },
            { Float2( -1.0f, -1.0f ) },
            { Float2( 1.0f, -1.0f ) }
        };

        GHI::BufferCreateInfo bufferCI = {};
        bufferCI.bImmutableStorage = true;
        bufferCI.SizeInBytes = sizeof( saqVertices );
        Saq.Initialize( bufferCI, saqVertices );
    }

    Core::ZeroMem( BufferBinding, sizeof( BufferBinding ) );
    Core::ZeroMem( TextureBindings, sizeof( TextureBindings ) );
    Core::ZeroMem( SamplerBindings, sizeof( SamplerBindings ) );

    ViewUniformBufferBinding = &BufferBinding[0];
    ViewUniformBufferBinding->BufferType = GHI::UNIFORM_BUFFER;
    ViewUniformBufferBinding->SlotIndex = 0;
    ViewUniformBufferBinding->pBuffer = FrameConstantBuffer->GetBuffer();

    InstanceUniformBufferBinding = &BufferBinding[1];
    InstanceUniformBufferBinding->BufferType = GHI::UNIFORM_BUFFER;
    InstanceUniformBufferBinding->SlotIndex = 1;
    InstanceUniformBufferBinding->pBuffer = nullptr;

    SkeletonBufferBinding = &BufferBinding[2];
    SkeletonBufferBinding->BufferType = GHI::UNIFORM_BUFFER;
    SkeletonBufferBinding->SlotIndex = 2;
    SkeletonBufferBinding->pBuffer = nullptr;

    CascadeBufferBinding = &BufferBinding[3];
    CascadeBufferBinding->BufferType = GHI::UNIFORM_BUFFER;
    CascadeBufferBinding->SlotIndex = 3;
    CascadeBufferBinding->pBuffer = FrameConstantBuffer->GetBuffer();

    LightBufferBinding = &BufferBinding[4];
    LightBufferBinding->BufferType = GHI::UNIFORM_BUFFER;
    LightBufferBinding->SlotIndex = 4;
    LightBufferBinding->pBuffer = FrameConstantBuffer->GetBuffer();

    IBLBufferBinding = &BufferBinding[5];
    IBLBufferBinding->BufferType = GHI::UNIFORM_BUFFER;
    IBLBufferBinding->SlotIndex = 5;
    IBLBufferBinding->pBuffer = FrameConstantBuffer->GetBuffer();

    for ( int i = 0 ; i < 16 ; i++ ) {
        TextureBindings[i].SlotIndex = i;
        SamplerBindings[i].SlotIndex = i;
    }

    Core::ZeroMem( &Resources, sizeof( Resources ) );
    Resources.Buffers = BufferBinding;
    Resources.NumBuffers = AN_ARRAY_SIZE( BufferBinding );

    Resources.Textures = TextureBindings;
    Resources.NumTextures = AN_ARRAY_SIZE( TextureBindings );

    Resources.Samplers = SamplerBindings;
    Resources.NumSamplers = AN_ARRAY_SIZE( SamplerBindings );




    /////////////////////////////////////////////////////////////////////
    // test
    /////////////////////////////////////////////////////////////////////
#if 1
    GHI::Texture cubemap;
    GHI::Texture cubemap2;
    {
        const char * Cubemap[6] = {
            "ClearSky/rt.bmp",
            "ClearSky/lt.bmp",
            "ClearSky/up.bmp",
            "ClearSky/dn.bmp",
            "ClearSky/bk.bmp",
            "ClearSky/ft.bmp"
        };
        const char * Cubemap2[6] = {
            "DarkSky/rt.tga",
            "DarkSky/lt.tga",
            "DarkSky/up.tga",
            "DarkSky/dn.tga",
            "DarkSky/bk.tga",
            "DarkSky/ft.tga"
        };
        AImage rt, lt, up, dn, bk, ft;
        AImage const * cubeFaces[6] = { &rt,&lt,&up,&dn,&bk,&ft };
        rt.Load( Cubemap[0], nullptr, IMAGE_PF_BGR32F );
        lt.Load( Cubemap[1], nullptr, IMAGE_PF_BGR32F );
        up.Load( Cubemap[2], nullptr, IMAGE_PF_BGR32F );
        dn.Load( Cubemap[3], nullptr, IMAGE_PF_BGR32F );
        bk.Load( Cubemap[4], nullptr, IMAGE_PF_BGR32F );
        ft.Load( Cubemap[5], nullptr, IMAGE_PF_BGR32F );
#if 0
        const float HDRI_Scale = 4.0f;
        const float HDRI_Pow = 1.1f;
#else
        const float HDRI_Scale = 1;
        const float HDRI_Pow = 1;
#endif
        for ( int i = 0 ; i < 6 ; i++ ) {
            float * HDRI = (float*)cubeFaces[i]->GetData();
            int count = cubeFaces[i]->GetWidth()*cubeFaces[i]->GetHeight()*3;
            for ( int j = 0; j < count ; j += 3 ) {
                HDRI[j] = Math::Pow( HDRI[j + 0] * HDRI_Scale, HDRI_Pow );
                HDRI[j + 1] = Math::Pow( HDRI[j + 1] * HDRI_Scale, HDRI_Pow );
                HDRI[j + 2] = Math::Pow( HDRI[j + 2] * HDRI_Scale, HDRI_Pow );
            }
        }
        int w = cubeFaces[0]->GetWidth();
        GHI::TextureStorageCreateInfo cubemapCI = {};
        cubemapCI.Type = GHI::TEXTURE_CUBE_MAP;
        cubemapCI.InternalFormat = GHI::INTERNAL_PIXEL_FORMAT_RGB32F;
        cubemapCI.Resolution.TexCubemap.Width = w;
        cubemapCI.NumLods = 1;
        cubemap.InitializeStorage( cubemapCI );
        for ( int face = 0 ; face < 6 ; face++ ) {
            float * pSrc = (float *)cubeFaces[face]->GetData();

            GHI::TextureRect rect = {};
            rect.Offset.Z = face;
            rect.Dimension.X = w;
            rect.Dimension.Y = w;
            rect.Dimension.Z = 1;

            cubemap.WriteRect( rect, GHI::PIXEL_FORMAT_FLOAT_BGR, w*w*3*sizeof( float ), 1, pSrc );
        }
        rt.Load( Cubemap2[0], nullptr, IMAGE_PF_BGR32F );
        lt.Load( Cubemap2[1], nullptr, IMAGE_PF_BGR32F );
        up.Load( Cubemap2[2], nullptr, IMAGE_PF_BGR32F );
        dn.Load( Cubemap2[3], nullptr, IMAGE_PF_BGR32F );
        bk.Load( Cubemap2[4], nullptr, IMAGE_PF_BGR32F );
        ft.Load( Cubemap2[5], nullptr, IMAGE_PF_BGR32F );
        w = cubeFaces[0]->GetWidth();
        cubemapCI.Resolution.TexCubemap.Width = w;
        cubemapCI.NumLods = 1;
        cubemap2.InitializeStorage( cubemapCI );
        for ( int face = 0 ; face < 6 ; face++ ) {
            float * pSrc = (float *)cubeFaces[face]->GetData();

            GHI::TextureRect rect = {};
            rect.Offset.Z = face;
            rect.Dimension.X = w;
            rect.Dimension.Y = w;
            rect.Dimension.Z = 1;

            cubemap2.WriteRect( rect, GHI::PIXEL_FORMAT_FLOAT_BGR, w*w*3*sizeof( float ), 1, pSrc );
        }
    }

    GHI::Texture * cubemaps[2] = { &cubemap, &cubemap2 };
#else

    Texture cubemap;
    {
        AImage img;

        //img.Load( "052_hdrmaps_com_free.exr", NULL, IMAGE_PF_RGB16F ); 
        //img.Load( "059_hdrmaps_com_free.exr", NULL, IMAGE_PF_RGB16F );
        img.Load( "087_hdrmaps_com_free.exr", NULL, IMAGE_PF_RGB16F );

        GHI::Texture source;
        GHI::TextureStorageCreateInfo createInfo = {};
        createInfo.Type = GHI::TEXTURE_2D;
        createInfo.InternalFormat = GHI::INTERNAL_PIXEL_FORMAT_RGB16F;
        createInfo.Resolution.Tex2D.Width = img.GetWidth();
        createInfo.Resolution.Tex2D.Height = img.GetHeight();
        createInfo.NumLods = 1;
        source.InitializeStorage( createInfo );
        source.Write( 0, GHI::PIXEL_FORMAT_HALF_RGB, img.GetWidth()*img.GetHeight()*3*2, 1, img.GetData() );

        const int cubemapResoultion = 1024;

        ACubemapGenerator cubemapGenerator;
        cubemapGenerator.Initialize();
        cubemapGenerator.Generate( cubemap, GHI::INTERNAL_PIXEL_FORMAT_RGB16F, cubemapResoultion, &source );

#if 0
        GHI::TextureRect rect;
        rect.Offset.Lod = 0;
        rect.Offset.X = 0;
        rect.Offset.Y = 0;
        rect.Dimension.X = cubemapResoultion;
        rect.Dimension.Y = cubemapResoultion;
        rect.Dimension.Z = 1;
        void * data = GHeapMemory.Alloc( cubemapResoultion*cubemapResoultion*3*sizeof( float ) );
        AFileStream f;
        for ( int i = 0 ; i < 6 ; i++ ) {
            rect.Offset.Z = i;
            cubemap.ReadRect( rect, GHI::PIXEL_FORMAT_FLOAT_RGB, cubemapResoultion*cubemapResoultion*3*sizeof( float ), 1, data );
            f.OpenWrite( Core::Fmt( "nightsky_%d.hdr", i ) );
            WriteHDR( f, cubemapResoultion, cubemapResoultion, 3, (float*)data );
        }
        GHeapMemory.Free( data );
#endif
    }

    Texture * cubemaps[] = { &cubemap };
#endif

    {
        AEnvProbeGenerator envProbeGenerator;
        envProbeGenerator.Initialize();
        envProbeGenerator.GenerateArray( PrefilteredMap, 7, AN_ARRAY_SIZE( cubemaps ), cubemaps );
        GHI::SamplerCreateInfo samplerCI;
        samplerCI.SetDefaults();
        samplerCI.Filter = GHI::FILTER_MIPMAP_BILINEAR;
        samplerCI.bCubemapSeamless = true;
        PrefilteredMapSampler = GDevice.GetOrCreateSampler( samplerCI );
        PrefilteredMapBindless.Initialize( &PrefilteredMap, PrefilteredMapSampler );
        PrefilteredMapBindless.MakeResident();
    }

    {
        AIrradianceGenerator irradianceGenerator;
        irradianceGenerator.Initialize();
        irradianceGenerator.GenerateArray( IrradianceMap, AN_ARRAY_SIZE( cubemaps ), cubemaps );
        GHI::SamplerCreateInfo samplerCI;
        samplerCI.SetDefaults();
        samplerCI.Filter = GHI::FILTER_LINEAR;
        samplerCI.bCubemapSeamless = true;
        IrradianceMapSampler = GDevice.GetOrCreateSampler( samplerCI );
        IrradianceMapBindless.Initialize( &IrradianceMap, IrradianceMapSampler );
        IrradianceMapBindless.MakeResident();
    }


    /////////////////////////////////////////////////////////////////////
}

void AFrameResources::Deinitialize() {
    ConstantBuffer.reset();
    FrameConstantBuffer.reset();
    Saq.Deinitialize();
    ClusterLookup.Deinitialize();
    ClusterItemTBO.Deinitialize();
    ClusterItemBuffer.Deinitialize();
    PrefilteredMapBindless.MakeNonResident();
    IrradianceMapBindless.MakeNonResident();
    PrefilteredMap.Deinitialize();
    IrradianceMap.Deinitialize();
}

void AFrameResources::SetViewUniforms() {
    size_t offset = FrameConstantBuffer->Allocate( sizeof( SViewUniformBuffer ) );

    SViewUniformBuffer * uniformData = (SViewUniformBuffer *)(FrameConstantBuffer->GetMappedMemory() + offset);

    const Float2 orthoMins( 0.0f, (float)GFrameData->CanvasHeight );
    const Float2 orthoMaxs( (float)GFrameData->CanvasWidth, 0.0f );

    uniformData->OrthoProjection = Float4x4::Ortho2DCC( orthoMins, orthoMaxs ); // TODO: calc ortho projection in render frontend

    uniformData->ViewProjection = GRenderView->ViewProjection;
    uniformData->InverseProjectionMatrix = GRenderView->InverseProjectionMatrix;

    uniformData->WorldNormalToViewSpace[0].X = GRenderView->NormalToViewMatrix[0][0];
    uniformData->WorldNormalToViewSpace[0].Y = GRenderView->NormalToViewMatrix[1][0];
    uniformData->WorldNormalToViewSpace[0].Z = GRenderView->NormalToViewMatrix[2][0];
    uniformData->WorldNormalToViewSpace[0].W = 0;

    uniformData->WorldNormalToViewSpace[1].X = GRenderView->NormalToViewMatrix[0][1];
    uniformData->WorldNormalToViewSpace[1].Y = GRenderView->NormalToViewMatrix[1][1];
    uniformData->WorldNormalToViewSpace[1].Z = GRenderView->NormalToViewMatrix[2][1];
    uniformData->WorldNormalToViewSpace[1].W = 0;

    uniformData->WorldNormalToViewSpace[2].X = GRenderView->NormalToViewMatrix[0][2];
    uniformData->WorldNormalToViewSpace[2].Y = GRenderView->NormalToViewMatrix[1][2];
    uniformData->WorldNormalToViewSpace[2].Z = GRenderView->NormalToViewMatrix[2][2];
    uniformData->WorldNormalToViewSpace[2].W = 0;

    uniformData->InvViewportSize.X = 1.0f / GRenderView->Width;
    uniformData->InvViewportSize.Y = 1.0f / GRenderView->Height;
    uniformData->ZNear = GRenderView->ViewZNear;
    uniformData->ZFar = GRenderView->ViewZFar;

    uniformData->GameRunningTimeSeconds = GRenderView->GameRunningTimeSeconds;
    uniformData->GameplayTimeSeconds = GRenderView->GameplayTimeSeconds;

    uniformData->DynamicResolutionRatioX = (float)GRenderView->Width / GFrameData->AllocSurfaceWidth;
    uniformData->DynamicResolutionRatioY = (float)GRenderView->Height / GFrameData->AllocSurfaceHeight;

    uniformData->ViewPosition = GRenderView->ViewPosition;
    uniformData->TimeDelta = GRenderView->GameplayTimeStep;

    uniformData->PostprocessBloomMix = Float4( RVPostprocessBloomParam0.GetFloat(),
                                               RVPostprocessBloomParam1.GetFloat(),
                                               RVPostprocessBloomParam2.GetFloat(),
                                               RVPostprocessBloomParam3.GetFloat() ) * RVPostprocessBloomScale.GetFloat();

    uniformData->BloomEnabled = RVPostprocessBloom;  // TODO: Get from GRenderView
    uniformData->ToneMappingExposure = RVPostprocessToneExposure.GetFloat();  // TODO: Get from GRenderView
    uniformData->ColorGrading = GRenderView->CurrentColorGradingLUT ? 1.0f : 0.0f;
    uniformData->FXAA = RVFxaa;
    uniformData->VignetteColorIntensity = GRenderView->VignetteColorIntensity;
    uniformData->VignetteOuterRadiusSqr = GRenderView->VignetteOuterRadiusSqr;
    uniformData->VignetteInnerRadiusSqr = GRenderView->VignetteInnerRadiusSqr;
    uniformData->ColorGradingAdaptationSpeed = GRenderView->ColorGradingAdaptationSpeed;
    uniformData->ViewBrightness = Math::Saturate( RVBrightness.GetFloat() );

    uniformData->uTemperatureScale.X = GRenderView->ColorGradingTemperatureScale.X;
    uniformData->uTemperatureScale.Y = GRenderView->ColorGradingTemperatureScale.Y;
    uniformData->uTemperatureScale.Z = GRenderView->ColorGradingTemperatureScale.Z;
    uniformData->uTemperatureScale.W = 0.0f;

    uniformData->uTemperatureStrength.X = GRenderView->ColorGradingTemperatureStrength.X;
    uniformData->uTemperatureStrength.Y = GRenderView->ColorGradingTemperatureStrength.Y;
    uniformData->uTemperatureStrength.Z = GRenderView->ColorGradingTemperatureStrength.Z;
    uniformData->uTemperatureStrength.W = 0.0f;

    uniformData->uGrain.X = GRenderView->ColorGradingGrain.X * 2.0f;
    uniformData->uGrain.Y = GRenderView->ColorGradingGrain.Y * 2.0f;
    uniformData->uGrain.Z = GRenderView->ColorGradingGrain.Z * 2.0f;
    uniformData->uGrain.W = 0.0f;

    uniformData->uGamma.X = 0.5f / Math::Max( GRenderView->ColorGradingGamma.X, 0.0001f );
    uniformData->uGamma.Y = 0.5f / Math::Max( GRenderView->ColorGradingGamma.Y, 0.0001f );
    uniformData->uGamma.Z = 0.5f / Math::Max( GRenderView->ColorGradingGamma.Z, 0.0001f );
    uniformData->uGamma.W = 0.0f;

    uniformData->uLift.X = GRenderView->ColorGradingLift.X * 2.0f - 1.0f;
    uniformData->uLift.Y = GRenderView->ColorGradingLift.Y * 2.0f - 1.0f;
    uniformData->uLift.Z = GRenderView->ColorGradingLift.Z * 2.0f - 1.0f;
    uniformData->uLift.W = 0.0f;

    uniformData->uPresaturation.X = GRenderView->ColorGradingPresaturation.X;
    uniformData->uPresaturation.Y = GRenderView->ColorGradingPresaturation.Y;
    uniformData->uPresaturation.Z = GRenderView->ColorGradingPresaturation.Z;
    uniformData->uPresaturation.W = 0.0f;

    uniformData->uLuminanceNormalization.X = GRenderView->ColorGradingBrightnessNormalization;
    uniformData->uLuminanceNormalization.Y = 0.0f;
    uniformData->uLuminanceNormalization.Z = 0.0f;
    uniformData->uLuminanceNormalization.W = 0.0f;

    uniformData->PrefilteredMapSampler = PrefilteredMapBindless.GetHandle();
    uniformData->IrradianceMapSampler = IrradianceMapBindless.GetHandle();

    uniformData->DebugMode = RVDebugRenderMode.GetInteger();

    uniformData->NumDirectionalLights = GRenderView->NumDirectionalLights;
    //GLogger.Printf( "GRenderView->FirstDirectionalLight: %d\n", GRenderView->FirstDirectionalLight );

    for ( int i = 0 ; i < GRenderView->NumDirectionalLights ; i++ ) {
        SDirectionalLightDef * light = GFrameData->DirectionalLights[GRenderView->FirstDirectionalLight + i];

        uniformData->LightDirs[i] = Float4( GRenderView->NormalToViewMatrix * (light->Matrix[2]), 0.0f );
        uniformData->LightColors[i] = light->ColorAndAmbientIntensity;
        uniformData->LightParameters[i][0] = light->RenderMask;
        uniformData->LightParameters[i][1] = light->FirstCascade;
        uniformData->LightParameters[i][2] = light->NumCascades;
    }

    ViewUniformBufferBinding->BindingOffset = offset;
    ViewUniformBufferBinding->BindingSize = sizeof( *uniformData );
}

void AFrameResources::UploadUniforms() {
    SkeletonBufferBinding->pBuffer = GPUBufferHandle( GFrameData->StreamBuffer );

    SetViewUniforms();

    // Cascade matrices
    const int totalCascades = MAX_DIRECTIONAL_LIGHTS * MAX_SHADOW_CASCADES;
    CascadeBufferBinding->BindingSize = totalCascades * 2 * sizeof( Float4x4 );
    CascadeBufferBinding->BindingOffset = FrameConstantBuffer->Allocate( CascadeBufferBinding->BindingSize );

    byte * pMemory = FrameConstantBuffer->GetMappedMemory() + CascadeBufferBinding->BindingOffset;

    Core::Memcpy( pMemory, GRenderView->LightViewProjectionMatrices, GRenderView->NumShadowMapCascades * sizeof( Float4x4 ) );

    pMemory += totalCascades * sizeof( Float4x4 );

    Core::Memcpy( pMemory, GRenderView->ShadowMapMatrices, GRenderView->NumShadowMapCascades * sizeof( Float4x4 ) );

    // Light buffer
    LightBufferBinding->BindingSize = GRenderView->LightData.TotalLights * sizeof( SClusterLight );
    LightBufferBinding->BindingOffset = FrameConstantBuffer->Allocate( LightBufferBinding->BindingSize );

    pMemory = FrameConstantBuffer->GetMappedMemory() + LightBufferBinding->BindingOffset;

    Core::Memcpy( pMemory, GRenderView->LightData.LightBuffer, LightBufferBinding->BindingSize );

    // IBL buffer
    IBLBufferBinding->BindingSize = GRenderView->LightData.TotalProbes * sizeof( SClusterProbe );
    IBLBufferBinding->BindingOffset = FrameConstantBuffer->Allocate( IBLBufferBinding->BindingSize );

    pMemory = FrameConstantBuffer->GetMappedMemory() + IBLBufferBinding->BindingOffset;

    Core::Memcpy( pMemory, GRenderView->LightData.Probes, IBLBufferBinding->BindingSize );

    // Write cluster data
    ClusterLookup.Write( 0,
                         GHI::PIXEL_FORMAT_UINT_RG,
                         sizeof( SClusterData )*MAX_FRUSTUM_CLUSTERS_X*MAX_FRUSTUM_CLUSTERS_Y*MAX_FRUSTUM_CLUSTERS_Z,
                         1,
                         GRenderView->LightData.ClusterLookup );

    ClusterItemBuffer.WriteRange( 0,
                                  sizeof( SClusterItemBuffer )*GRenderView->LightData.TotalItems,
                                  GRenderView->LightData.ItemBuffer );
}

ACircularBuffer::ACircularBuffer( size_t InBufferSize )
    : BufferSize( InBufferSize )
{
    GHI::BufferCreateInfo bufferCI = {};

    bufferCI.SizeInBytes = BufferSize * SWAP_CHAIN_SIZE;

    bufferCI.ImmutableStorageFlags = (GHI::IMMUTABLE_STORAGE_FLAGS)(GHI::IMMUTABLE_MAP_WRITE | GHI::IMMUTABLE_MAP_PERSISTENT | GHI::IMMUTABLE_MAP_COHERENT);
    bufferCI.bImmutableStorage = true;

    Buffer.Initialize( bufferCI );

    pMappedMemory = Buffer.Map( GHI::MAP_TRANSFER_WRITE,
                                GHI::MAP_NO_INVALIDATE,//GHI::MAP_INVALIDATE_ENTIRE_BUFFER,
                                GHI::MAP_PERSISTENT_COHERENT,
                                false, // flush explicit
                                false ); // unsynchronized

    if ( !pMappedMemory ) {
        CriticalError( "ACircularBuffer::ctor: cannot initialize persistent mapped buffer size %d\n", bufferCI.SizeInBytes );
    }

    for ( int i = 0 ; i < SWAP_CHAIN_SIZE ; i++ ) {
        ChainBuffer[i].UsedMemory = 0;
        ChainBuffer[i].Sync = 0;
    }

    BufferIndex = 0;
}

ACircularBuffer::~ACircularBuffer()
{
    for ( int i = 0 ; i < SWAP_CHAIN_SIZE ; i++ ) {
        Wait( ChainBuffer[i].Sync );
        Cmd.RemoveSync( ChainBuffer[i].Sync );
    }

    Buffer.Unmap();
}

size_t ACircularBuffer::Allocate( size_t InSize )
{
    AN_ASSERT( InSize > 0 && InSize <= BufferSize );

    SChainBuffer * pChainBuffer = &ChainBuffer[BufferIndex];

    size_t alignedOffset = Align( pChainBuffer->UsedMemory, GDevice.GetUniformBufferOffsetAlignment() );

    if ( alignedOffset + InSize > BufferSize ) {
        pChainBuffer = Swap();
        alignedOffset = 0;
    }

    pChainBuffer->UsedMemory = alignedOffset + InSize;

    alignedOffset += BufferIndex * BufferSize;

    return alignedOffset;
}

ACircularBuffer::SChainBuffer * ACircularBuffer::Swap()
{
    SChainBuffer * pCurrent = &ChainBuffer[BufferIndex];
    Cmd.RemoveSync( pCurrent->Sync );

    pCurrent->Sync = Cmd.FenceSync();

    BufferIndex = (BufferIndex + 1) % SWAP_CHAIN_SIZE;

    pCurrent = &ChainBuffer[BufferIndex];
    pCurrent->UsedMemory = 0;

    Wait( pCurrent->Sync );

    GLogger.Printf( "Swap at %d\n", GFrameData->FrameNumber );

    return pCurrent;
}

void ACircularBuffer::Wait( GHI::SyncObject Sync )
{
    const uint64_t timeOutNanoseconds = 1;
    if ( Sync ) {
        GHI::CLIENT_WAIT_STATUS status;
        do {
            status = Cmd.ClientWait( Sync, timeOutNanoseconds );
        } while ( status != GHI::CLIENT_WAIT_ALREADY_SIGNALED && status != GHI::CLIENT_WAIT_CONDITION_SATISFIED );
    }
}

AFrameConstantBuffer::AFrameConstantBuffer( size_t InBufferSize )
    : BufferSize( InBufferSize )
{
    GHI::BufferCreateInfo bufferCI = {};

    bufferCI.SizeInBytes = BufferSize * SWAP_CHAIN_SIZE;

    bufferCI.ImmutableStorageFlags = (GHI::IMMUTABLE_STORAGE_FLAGS)(GHI::IMMUTABLE_MAP_WRITE | GHI::IMMUTABLE_MAP_PERSISTENT | GHI::IMMUTABLE_MAP_COHERENT);
    bufferCI.bImmutableStorage = true;

    Buffer.Initialize( bufferCI );

    pMappedMemory = Buffer.Map( GHI::MAP_TRANSFER_WRITE,
                                GHI::MAP_NO_INVALIDATE,//GHI::MAP_INVALIDATE_ENTIRE_BUFFER,
                                GHI::MAP_PERSISTENT_COHERENT,
                                false, // flush explicit
                                false ); // unsynchronized

    if ( !pMappedMemory ) {
        CriticalError( "AFrameConstantBuffer::ctor: cannot initialize persistent mapped buffer size %d\n", bufferCI.SizeInBytes );
    }

    for ( int i = 0 ; i < SWAP_CHAIN_SIZE ; i++ ) {
        ChainBuffer[i].UsedMemory = 0;
        ChainBuffer[i].Sync = 0;
    }

    BufferIndex = 0;
}

AFrameConstantBuffer::~AFrameConstantBuffer()
{
    for ( int i = 0 ; i < SWAP_CHAIN_SIZE ; i++ ) {
        Wait( ChainBuffer[i].Sync );
        Cmd.RemoveSync( ChainBuffer[i].Sync );
    }

    Buffer.Unmap();
}

size_t AFrameConstantBuffer::Allocate( size_t InSize )
{
    AN_ASSERT( InSize > 0 && InSize <= BufferSize );

    SChainBuffer * pChainBuffer = &ChainBuffer[BufferIndex];

    size_t alignedOffset = Align( pChainBuffer->UsedMemory, GDevice.GetUniformBufferOffsetAlignment() );

    if ( alignedOffset + InSize > BufferSize ) {
        CriticalError( "AFrameConstantBuffer::Allocate: failed on allocation of %d bytes\nIncrease buffer size\n", InSize );
    }

    pChainBuffer->UsedMemory = alignedOffset + InSize;

    alignedOffset += BufferIndex * BufferSize;

    return alignedOffset;
}

void AFrameConstantBuffer::Begin()
{
    Wait( ChainBuffer[BufferIndex].Sync );
}

void AFrameConstantBuffer::End()
{
    SChainBuffer * pCurrent = &ChainBuffer[BufferIndex];
    Cmd.RemoveSync( pCurrent->Sync );

    pCurrent->Sync = Cmd.FenceSync();

    BufferIndex = (BufferIndex + 1) % SWAP_CHAIN_SIZE;

    pCurrent = &ChainBuffer[BufferIndex];
    pCurrent->UsedMemory = 0;
}

void AFrameConstantBuffer::Wait( GHI::SyncObject Sync )
{
    const uint64_t timeOutNanoseconds = 1;
    if ( Sync ) {
        GHI::CLIENT_WAIT_STATUS status;
        do {
            status = Cmd.ClientWait( Sync, timeOutNanoseconds );
        } while ( status != GHI::CLIENT_WAIT_ALREADY_SIGNALED && status != GHI::CLIENT_WAIT_CONDITION_SATISFIED );
    }
}

}
