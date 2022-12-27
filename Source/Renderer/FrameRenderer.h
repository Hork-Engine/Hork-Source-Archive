/*

Hork Engine Source Code

MIT License

Copyright (C) 2017-2022 Alexander Samusev.

This file is part of the Hork Engine Source Code.

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

#include "DepthRenderer.h"
#include "LightRenderer.h"
#include "DebugDrawRenderer.h"
#include "WireframeRenderer.h"
#include "NormalsRenderer.h"
#include "BloomRenderer.h"
#include "ExposureRenderer.h"
#include "ColorGradingRenderer.h"
#include "PostprocessRenderer.h"
#include "FxaaRenderer.h"
#include "SmaaRenderer.h"
#include "SSAORenderer.h"
#include "ShadowMapRenderer.h"

HK_NAMESPACE_BEGIN

class FrameRenderer : public RefCounted
{
public:
    FrameRenderer();

    void Render(RenderCore::FrameGraph& FrameGraph, bool bVirtualTexturing, class VirtualTextureCache* PhysCacheVT);

    OmnidirectionalShadowMapPool const& GetOmniShadowMapPool() const { return OmniShadowMapPool; }

private:
    void AddLinearizeDepthPass(RenderCore::FrameGraph& FrameGraph, RenderCore::FGTextureProxy* DepthTexture, RenderCore::FGTextureProxy** ppLinearDepth);

    void AddReconstrutNormalsPass(RenderCore::FrameGraph& FrameGraph, RenderCore::FGTextureProxy* LinearDepth, RenderCore::FGTextureProxy** ppNormalTexture);

    void AddMotionBlurPass(RenderCore::FrameGraph&    FrameGraph,
                           RenderCore::FGTextureProxy*  LightTexture,
                           RenderCore::FGTextureProxy*  VelocityTexture,
                           RenderCore::FGTextureProxy*  LinearDepth,
                           RenderCore::FGTextureProxy** ppResultTexture);

    void AddOutlinePass(RenderCore::FrameGraph& FrameGraph, RenderCore::FGTextureProxy** ppOutlineTexture);
    void AddOutlineOverlayPass(RenderCore::FrameGraph& FrameGraph, RenderCore::FGTextureProxy* RenderTarget, RenderCore::FGTextureProxy* OutlineMaskTexture);
    void AddCopyPass(RenderCore::FrameGraph& FrameGraph, RenderCore::FGTextureProxy* Source, RenderCore::FGTextureProxy* Dest);

    ShadowMapRenderer    ShadowMapRenderer;
    LightRenderer        LightRenderer;
    DebugDrawRenderer    DebugDrawRenderer;
    BloomRenderer        BloomRenderer;
    ExposureRenderer     ExposureRenderer;
    ColorGradingRenderer ColorGradingRenderer;
    PostprocessRenderer  PostprocessRenderer;
    FxaaRenderer         FxaaRenderer;
    SmaaRenderer         SmaaRenderer;
    SSAORenderer         SSAORenderer;

    OmnidirectionalShadowMapPool OmniShadowMapPool;

    TRef<RenderCore::IPipeline> LinearDepthPipe;
    TRef<RenderCore::IPipeline> LinearDepthPipe_ORTHO;
    TRef<RenderCore::IPipeline> ReconstructNormalPipe;
    TRef<RenderCore::IPipeline> ReconstructNormalPipe_ORTHO;
    TRef<RenderCore::IPipeline> MotionBlurPipeline;
    TRef<RenderCore::IPipeline> OutlineBlurPipe;
    TRef<RenderCore::IPipeline> OutlineApplyPipe;
    TRef<RenderCore::IPipeline> CopyPipeline;
};

HK_NAMESPACE_END
