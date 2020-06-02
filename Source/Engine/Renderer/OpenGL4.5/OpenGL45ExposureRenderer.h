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

#pragma once

#include "FrameGraph.h"

namespace OpenGL45 {

class AExposureRenderer {
public:
    AExposureRenderer();

    AFrameGraphTextureStorage * AddPass( AFrameGraph & FrameGraph, AFrameGraphTextureStorage * SourceTexture_R );

    GHI::Texture & GetDefaultLuminance() { return DefaultLuminance; }

private:
    void CreateSampler();

    GHI::Pipeline MakeLuminanceMapPipe;
    GHI::Pipeline SumLuminanceMapPipe;
    GHI::Pipeline DynamicExposurePipe;
    GHI::Sampler LuminanceSampler;
    GHI::Texture Luminance64;
    GHI::Texture Luminance32;
    GHI::Texture Luminance16;
    GHI::Texture Luminance8;
    GHI::Texture Luminance4;
    GHI::Texture Luminance2;
    GHI::Texture DefaultLuminance;
};

}
