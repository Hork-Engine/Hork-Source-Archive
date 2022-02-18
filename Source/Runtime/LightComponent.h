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

#include "SceneComponent.h"
#include "AnimationPattern.h"

class ALightComponent : public ASceneComponent
{
    HK_COMPONENT(ALightComponent, ASceneComponent)

public:
    virtual void SetEnabled(bool _Enabled);

    bool IsEnabled() const { return bEnabled; }

    void SetAnimation(const char* _Pattern, float _Speed = 1.0f, float _Quantizer = 0.0f);

    void SetAnimation(AAnimationPattern* _Animation);

    AAnimationPattern* GetAnimation() { return Animation; }

    void SetAnimationTime(float _Time);

    float GetAnimationTime() const { return AnimTime; }

protected:
    ALightComponent();

    void TickComponent(float _TimeStep) override;

    float GetAnimationBrightness() const { return AnimationBrightness; }

    mutable bool bEffectiveColorDirty = true;

private:
    bool                    bEnabled = true;
    TRef<AAnimationPattern> Animation;
    float                   AnimTime            = 0.0f;
    float                   AnimationBrightness = 1.0f;
};
