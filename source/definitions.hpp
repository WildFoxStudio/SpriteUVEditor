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

#include <cmath>
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
    constexpr static uint32_t ZOOM_FRACTBITS{ 8 };
    constexpr static uint32_t ZOOM_FRACT{ 1 << ZOOM_FRACTBITS };
    /*Fixed point float*/
    uint32_t zoom{ 1u + ZOOM_FRACT };
    /*Fixed point float*/
    uint32_t prevZoom{ zoom };
    float    fitZoom{ 1.f };
    Vec2     pan{};

    Vector2                GetZoomedPan() const { return Vector2{ pan.x * GetZoomFactor(), pan.y * GetZoomFactor() }; }
    inline uint32_t        GetMinZoom() const { return ToFixed(fitZoom * 0.1f); };
    inline uint32_t        GetMaxZoom() const { return ToFixed(fitZoom * 100); };
    inline float           GetZoomFactor() const { return static_cast<float>(zoom) / static_cast<float>(ZOOM_FRACT); }
    inline float           GetPrevZoomFactor() const { return static_cast<float>(prevZoom) / static_cast<float>(ZOOM_FRACT); }
    inline void            SetZoomFactor(float value) { zoom = static_cast<uint32_t>(std::round(value * ZOOM_FRACT)); }
    inline static uint32_t ToFixed(float value) { return static_cast<uint32_t>(std::round(value * ZOOM_FRACT)); }
    inline static int32_t  MultiplyFixed(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> ZOOM_FRACTBITS); }
    inline static uint32_t DivideFixed(uint32_t a, uint32_t b)
    {
        // We cast to uint64_t to prevent overflow during the multiplication
        return (uint32_t)(((uint64_t)a * ZOOM_FRACT) / b);
    }

    inline void SafelyClampZoom() { zoom = std::clamp(zoom, GetMinZoom(), GetMaxZoom()); }

    inline void SafelyClampPan()
    {
        pan.x = std::clamp(pan.x, -std::numeric_limits<int16_t>::max(), +std::numeric_limits<int16_t>::max());
        pan.y = std::clamp(pan.y, -std::numeric_limits<int16_t>::max(), +std::numeric_limits<int16_t>::max());
    }

    inline Rectangle TransformRect(const Rectangle& rect) const
    {
        const auto z{ GetZoomFactor() };
        Rectangle  transformedRect{};
        transformedRect.x      = rect.x * z + pan.x;
        transformedRect.y      = rect.y * z + pan.y;
        transformedRect.width  = rect.width * z;
        transformedRect.height = rect.height * z;
        return transformedRect;
    }

    inline static float ZoomFitIntoRect(int32_t texWidth, int32_t texHeight, const Rect& targetRect)
    {
        const auto scaleX{ targetRect.w / static_cast<float>(texWidth) };
        const auto scaleY{ targetRect.h / static_cast<float>(texHeight) };
        return std::min(scaleX, scaleY);
    }
};
