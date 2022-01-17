/*

Angie Engine Source Code

MIT License

Copyright (C) 2017-2022 Alexander Samusev.

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

#include "Controller.h"
#include <Platform/Logger.h>

AN_CLASS_META(AController)

AController::AController()
{
    bCanEverTick        = true;
    bTickEvenWhenPaused = true;
}

void AController::Tick(float TimeStep)
{
    Super::Tick(TimeStep);

    if (Pawn && Pawn->IsPendingKill())
    {
        SetPawn(nullptr);
    }
}

void AController::SetPawn(AActor* _Pawn)
{
    if (IsSame(Pawn, _Pawn))
    {
        return;
    }

    if (_Pawn && _Pawn->Controller)
    {
        GLogger.Printf("Pawn already controlled by other controller\n");
        return;
    }

    if (Pawn)
    {
        Pawn->Controller = nullptr;
    }

    Pawn = _Pawn;

    if (Pawn)
    {
        Pawn->Controller = this;
    }

    OnPawnChanged();
}
