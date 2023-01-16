/*

Hork Engine Source Code

MIT License

Copyright (C) 2017-2023 Alexander Samusev.

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

#include "UIObject.h"

#include <Runtime/Canvas/Canvas.h>

HK_NAMESPACE_BEGIN

class UICursor : public UIObject
{
    UI_CLASS(UICursor, UIObject)

public:
    virtual void Draw(Canvas& canvas, Float2 const& position) = 0;
};

class UIDefaultCursor : public UICursor
{
    UI_CLASS(UICursor, UICursor)

public:
    DRAW_CURSOR DrawCursor = DRAW_CURSOR_ARROW;
    Color4      FillColor  = Color4::White();
    Color4      BorderColor = Color4::Black();
    bool        bDropShadow = true;

    UIDefaultCursor& WithDrawCursor(DRAW_CURSOR drawCursor)
    {
        DrawCursor = drawCursor;
        return *this;
    }

    UIDefaultCursor& WithFillColor(Color4 const& fillColor)
    {
        FillColor = fillColor;
        return *this;
    }

    UIDefaultCursor& WithBorderColor(Color4 const& borderColor)
    {
        BorderColor = borderColor;
        return *this;
    }

    UIDefaultCursor& WithDropShadow(bool dropShadow)
    {
        bDropShadow = dropShadow;
        return *this;
    }

    void Draw(Canvas& canvas, Float2 const& position) override;
};

HK_NAMESPACE_END
