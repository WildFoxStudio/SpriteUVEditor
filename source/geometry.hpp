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

#include "raylib.h"

#include <cstdint>

template <typename T> struct TVec2 {
	T x{}, y{};
};

template <typename T> struct TRect {
	T x{}, y{}, w{}, h{};
};

// Use int32_t for all coordinates and sizes since raygui/raylib use int for those
// Also avoid floating point precision issues for pixel coordinates
using Vec2 = TVec2<int32_t>;
using Rect = TRect<int32_t>;

namespace to
{

inline Vector2 Vector2_(const Vec2 &vec)
{
	return { static_cast<float>(vec.x), static_cast<float>(vec.y) };
}

inline Rectangle Rectangle_(const Rect &rect)
{
	return { static_cast<float>(rect.x), static_cast<float>(rect.y),
		 static_cast<float>(rect.w), static_cast<float>(rect.h) };
}

}

namespace from
{

inline Vec2 Vector2_(const Vector2 &vec)
{
	return { static_cast<int32_t>(vec.x), static_cast<int32_t>(vec.y) };
}

inline Rectangle Rectangle_(const Rect &rect)
{
	return { static_cast<float>(rect.x), static_cast<float>(rect.y),
		 static_cast<float>(rect.w), static_cast<float>(rect.h) };
}

}
