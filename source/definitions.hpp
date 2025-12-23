/*
MIT License

Copyright (c) 2025 Kirichenko Stanislav

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

#include <cstdint>

#include "geometry.hpp"

enum class EAnimationType
{
    SPRITESHEET,
    KEYFRAME,
};

enum class EModalType
{
    NONE,
    CREATE_ANIMATION,
    CONFIRM_DELETE,
};

enum EControlIndex : int32_t
{
    NONE   = 0,
    TOP    = 1 << 1,
    BOTTOM = 1 << 2,
    LEFT   = 1 << 3,
    RIGHT  = 1 << 4,
    CENTER = TOP | BOTTOM | LEFT | RIGHT,
};

struct View
{
    float zoom{ 1.f };
    float prevZoom{ 1.f };
    float fitZoom{ 1.f };
    Vec2  pan{};

    Rectangle TransformRect(const Rectangle& rect) const
    {
        Rectangle transformedRect{};
        transformedRect.x      = rect.x * zoom + pan.x;
        transformedRect.y      = rect.y * zoom + pan.y;
        transformedRect.width  = rect.width * zoom;
        transformedRect.height = rect.height * zoom;
        return transformedRect;
    }
};
