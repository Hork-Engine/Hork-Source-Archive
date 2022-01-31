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

#include <Geometry/Triangulator.h>

#include "glutess/glutess.h"

ATriangulatorBase::ATriangulatorBase()
{
    tesselator_ = gluNewTess();
}

ATriangulatorBase::~ATriangulatorBase()
{
    gluDeleteTess(static_cast<GLUtesselator*>(tesselator_));
}

void ATriangulatorBase::SetCallback(uint32_t name, SCallback callback)
{
    gluTessCallback(static_cast<GLUtesselator*>(tesselator_), name, callback);
}

void ATriangulatorBase::SetBoundary(bool flag)
{
    gluTessProperty(static_cast<GLUtesselator*>(tesselator_), GLU_TESS_BOUNDARY_ONLY, flag);
}

void ATriangulatorBase::SetNormal(Double3 const& normal)
{
    gluTessNormal(static_cast<GLUtesselator*>(tesselator_), normal.X, normal.Y, normal.Z);
}

void ATriangulatorBase::BeginPolygon(void* data)
{
    gluTessBeginPolygon(static_cast<GLUtesselator*>(tesselator_), data);
}

void ATriangulatorBase::EndPolygon()
{
    gluTessEndPolygon(static_cast<GLUtesselator*>(tesselator_));
}

void ATriangulatorBase::BeginContour()
{
    gluTessBeginContour(static_cast<GLUtesselator*>(tesselator_));
}

void ATriangulatorBase::EndContour()
{
    gluTessEndContour(static_cast<GLUtesselator*>(tesselator_));
}

void ATriangulatorBase::ProcessVertex(Double3& vertex, const void* data)
{
    gluTessVertex(static_cast<GLUtesselator*>(tesselator_), &vertex.X, const_cast<void*>(data));
}
