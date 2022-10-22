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

#include "UIWidget.h"

class UIText : public UIObject
{
    UI_CLASS(UIText, UIObject)

public:
    AString           Text;
    TRef<AFont>       Font;
    float             FontSize = 14;
    float             FontBlur = 0;
    float             LetterSpacing = 0;
    float             LineHeight = 1; // The line height is specified as multiple of font size.
    HALIGNMENT        HAlignment = HALIGNMENT_LEFT;
    VALIGNMENT        VAlignment = VALIGNMENT_TOP;
    Color4            Color;
    Float2            ShadowOffset = Float2(2,2);
    float             ShadowBlur   = 2;
    bool              bWrap        = false;
    bool              bDropShadow  = true;   

    Float2 GetTextBoxSize(float breakRowWidth) const;

    void Draw(ACanvas& canvas, Float2 const& boxMins, Float2 const& boxMaxs);
};
