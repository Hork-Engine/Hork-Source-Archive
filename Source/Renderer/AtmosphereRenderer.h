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

#include <RenderCore/ImmediateContext.h>
#include <Geometry/VectorMath.h>

class AtmosphereRenderer
{
public:
    AtmosphereRenderer();

    void Render(TEXTURE_FORMAT Format, int CubemapWidth, Float3 const& LightDir, TRef<RenderCore::ITexture>* ppTexture);

private:
    struct ConstantData
    {
        Float4x4 Transform[6];
        Float4   LightDir;
    };
    TRef<RenderCore::IBuffer>   ConstantBuffer;
    ConstantData                ConstantBufferData;
    TRef<RenderCore::IPipeline> Pipeline;
};
