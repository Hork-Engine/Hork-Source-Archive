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

#include "IrradianceGenerator.h"
#include "RenderLocal.h"

#include <Core/Public/PodVector.h>

using namespace RenderCore;

static const TEXTURE_FORMAT TEX_FORMAT_IRRADIANCE = TEXTURE_FORMAT_RGB16F; // TODO: try compression

AIrradianceGenerator::AIrradianceGenerator()
{
    SBufferDesc bufferCI = {};
    bufferCI.bImmutableStorage = true;
    bufferCI.ImmutableStorageFlags = IMMUTABLE_DYNAMIC_STORAGE;
    bufferCI.SizeInBytes = sizeof( SConstantData );
    GDevice->CreateBuffer( bufferCI, nullptr, &ConstantBuffer );

    Float4x4 const * cubeFaceMatrices = Float4x4::GetCubeFaceMatrices();
    Float4x4 projMat = Float4x4::PerspectiveRevCC( Math::_HALF_PI, 1.0f, 1.0f, 0.1f, 100.0f );

    for ( int faceIndex = 0 ; faceIndex < 6 ; faceIndex++ ) {
        ConstantBufferData.Transform[faceIndex] = projMat * cubeFaceMatrices[faceIndex];
    }

    SPipelineDesc pipelineCI;

    SPipelineInputAssemblyInfo & ia = pipelineCI.IA;
    ia.Topology = PRIMITIVE_TRIANGLES;

    SDepthStencilStateInfo & depthStencil = pipelineCI.DSS;
    depthStencil.bDepthEnable = false;
    depthStencil.DepthWriteMask = DEPTH_WRITE_DISABLE;

    SVertexBindingInfo vertexBindings[] =
    {
        {
            0,                              // vertex buffer binding
            sizeof( Float3 ),               // vertex stride
            INPUT_RATE_PER_VERTEX,          // per vertex / per instance
        }
    };

    SVertexAttribInfo vertexAttribs[] =
    {
        {
            "InPosition",
            0,
            0,          // vertex buffer binding
            VAT_FLOAT3,
            VAM_FLOAT,
            0,
            0
        }
    };

    CreateVertexShader( "gen/irradiancegen.vert", vertexAttribs, AN_ARRAY_SIZE( vertexAttribs ), pipelineCI.pVS );
    CreateGeometryShader( "gen/irradiancegen.geom", pipelineCI.pGS );
    CreateFragmentShader( "gen/irradiancegen.frag", pipelineCI.pFS );

    pipelineCI.NumVertexBindings = AN_ARRAY_SIZE( vertexBindings );
    pipelineCI.pVertexBindings = vertexBindings;
    pipelineCI.NumVertexAttribs = AN_ARRAY_SIZE( vertexAttribs );
    pipelineCI.pVertexAttribs = vertexAttribs;

    SSamplerDesc samplerCI;
    samplerCI.Filter = FILTER_LINEAR;
    samplerCI.bCubemapSeamless = true;

    SBufferInfo buffers[1];
    buffers[0].BufferBinding = BUFFER_BIND_CONSTANT;

    pipelineCI.ResourceLayout.Samplers = &samplerCI;
    pipelineCI.ResourceLayout.NumSamplers = 1;
    pipelineCI.ResourceLayout.NumBuffers = AN_ARRAY_SIZE( buffers );
    pipelineCI.ResourceLayout.Buffers = buffers;

    GDevice->CreatePipeline( pipelineCI, &Pipeline );
}

void AIrradianceGenerator::GenerateArray( int _CubemapsCount, ITexture ** _Cubemaps, TRef< RenderCore::ITexture > * ppTextureArray )
{
    int size = 32;

    GDevice->CreateTexture(STextureDesc()
                               .SetFormat(TEX_FORMAT_IRRADIANCE)
                               .SetResolution(STextureResolutionCubemapArray(size, _CubemapsCount)),
                           ppTextureArray);

    AFrameGraph frameGraph( GDevice );

    FGTextureProxy* pCubemapArrayProxy = frameGraph.AddExternalResource<FGTextureProxy>("CubemapArray", *ppTextureArray);

    TRef< IResourceTable > resourceTbl;
    GDevice->CreateResourceTable( &resourceTbl );

    resourceTbl->BindBuffer( 0, ConstantBuffer );

    SViewport viewport = {};
    viewport.MaxDepth  = 1;

    ARenderPass & pass = frameGraph.AddTask< ARenderPass >( "Irradiance gen pass" );

    pass.SetRenderArea( size, size );

    pass.SetColorAttachment(
        STextureAttachment(pCubemapArrayProxy)
        .SetLoadOp( ATTACHMENT_LOAD_OP_DONT_CARE )
    );

    pass.AddSubpass( { 0 }, // color attachments
                    [&](ARenderPassContext& RenderPassContext, ACommandBuffer& CommandBuffer)
                     {
                         rcmd->BindResourceTable( resourceTbl );

                         for ( int cubemapIndex = 0; cubemapIndex < _CubemapsCount; cubemapIndex++ ) {
                             ConstantBufferData.Index.X = cubemapIndex * 6; // Offset for cubemap array layer

                             rcmd->WriteBuffer(ConstantBuffer, &ConstantBufferData);

                             resourceTbl->BindTexture( 0, _Cubemaps[cubemapIndex] );

                             // Draw six faces in one draw call
                             DrawSphere( Pipeline, 6 );
                         }
                     } );

    frameGraph.Build();
    frameGraph.ExportGraphviz( "framegraph.graphviz" );
    //frameGraph.Execute( rcmd );
    rcmd->ExecuteFrameGraph(&frameGraph);
}

void AIrradianceGenerator::Generate( ITexture * _SourceCubemap, TRef< RenderCore::ITexture > * ppTexture )
{
    int size = 32;

    GDevice->CreateTexture(STextureDesc()
                               .SetFormat(TEX_FORMAT_IRRADIANCE)
                               .SetResolution(STextureResolutionCubemap(size)),
                           ppTexture);

    AFrameGraph frameGraph( GDevice );

    FGTextureProxy* pCubemapProxy = frameGraph.AddExternalResource<FGTextureProxy>("Cubemap", *ppTexture);

    TRef< IResourceTable > resourceTbl;
    GDevice->CreateResourceTable( &resourceTbl );

    resourceTbl->BindBuffer( 0, ConstantBuffer );

    SViewport viewport = {};
    viewport.MaxDepth  = 1;

    ARenderPass & pass = frameGraph.AddTask< ARenderPass >( "Irradiance gen pass" );

    pass.SetRenderArea( size, size );

    pass.SetColorAttachment(
        STextureAttachment(pCubemapProxy)
        .SetLoadOp( ATTACHMENT_LOAD_OP_DONT_CARE )
    );

    pass.AddSubpass( { 0 }, // color attachments
                    [&](ARenderPassContext& RenderPassContext, ACommandBuffer& CommandBuffer)
                     {
                         ConstantBufferData.Index.X = 0;

                         rcmd->WriteBuffer(ConstantBuffer, &ConstantBufferData);

                         resourceTbl->BindTexture( 0, _SourceCubemap );

                         rcmd->BindResourceTable( resourceTbl );

                         // Draw six faces in one draw call
                         DrawSphere( Pipeline, 6 );
                     } );

    frameGraph.Build();
    frameGraph.ExportGraphviz( "framegraph.graphviz" );
    //frameGraph.Execute( rcmd );
    rcmd->ExecuteFrameGraph(&frameGraph);
}
