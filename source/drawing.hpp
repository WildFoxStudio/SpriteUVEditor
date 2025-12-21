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

#include "definitions.hpp"

#include <cmath>

void DrawDashedLine(Vector2 start, Vector2 end, float dashLength,
		    float gapLength, float thickness, Color color)
{
	float dx = end.x - start.x;
	float dy = end.y - start.y;
	float length = sqrtf(dx * dx + dy * dy);

	float dirX = dx / length;
	float dirY = dy / length;

	float drawn = 0.0f;
	while (drawn < length) {
		float segment = fminf(dashLength, length - drawn);

		Vector2 a = { start.x + dirX * drawn, start.y + dirY * drawn };
		Vector2 b = { start.x + dirX * (drawn + segment),
			      start.y + dirY * (drawn + segment) };

		DrawLineEx(a, b, thickness, color);

		drawn += dashLength + gapLength;
	}
}

void DrawUVRectDashed(Rectangle rect, View view, float fitZoom)
{
	constexpr Color dashColor{ DARKBLUE };
	rect = view.TransformRect(rect);

	constexpr float baseThickness{ 1.8f };
	constexpr float dashLen{ 10 * baseThickness };
	constexpr float dashGap{ 2 * baseThickness };
	const auto thickness{ baseThickness * fitZoom };

	//DrawRectangleLinesEx(rect, 1.f, BLACK);
	DrawDashedLine({ rect.x, rect.y }, { rect.x + rect.width, rect.y },
		       dashLen, dashGap, thickness, dashColor);
	DrawDashedLine({ rect.x, rect.y + rect.height },
		       { rect.x + rect.width, rect.y + rect.height }, dashLen,
		       dashGap, thickness, dashColor);

	DrawDashedLine({ rect.x, rect.y }, { rect.x, rect.y + rect.height },
		       dashLen, dashGap, thickness, dashColor);
	DrawDashedLine({ rect.x + rect.width, rect.y },
		       { rect.x + rect.width, rect.y + rect.height }, dashLen,
		       dashGap, thickness, dashColor);
}

bool DrawControl(Vector2 origin, float controlExtent, Color baseColor)
{
	constexpr Color focusedColor{ BLUE };
	const Rectangle cRect{ origin.x - controlExtent,
			       origin.y - controlExtent, controlExtent * 2.f,
			       controlExtent * 2.f };

	const bool mouseHover =
		CheckCollisionPointRec(GetMousePosition(), cRect);

	DrawRectangleRec(cRect, mouseHover ? focusedColor : baseColor);

	return mouseHover;
}

int32_t DrawUvRectControlsGetControlIndex(Rectangle rect, View view,
					  float controlExtent)
{
	constexpr Color controlColor{ DARKBLUE };

	rect = view.TransformRect(rect);

	int32_t index{ EControlIndex::NONE };
	if (DrawControl({ rect.x + rect.width * .5f, rect.y }, controlExtent,
			controlColor)) {
		index = { EControlIndex::TOP };
	}
	if (DrawControl({ rect.x, rect.y }, controlExtent, controlColor)) {
		index = { EControlIndex::TOP | EControlIndex::LEFT };
	}
	if (DrawControl({ rect.x + rect.width, rect.y }, controlExtent,
			controlColor)) {
		index = { EControlIndex::TOP | EControlIndex::RIGHT };
	}
	if (DrawControl({ rect.x + rect.width * .5f, rect.y + rect.height },
			controlExtent, controlColor)) {
		index = { EControlIndex::BOTTOM };
	}
	if (DrawControl({ rect.x, rect.y + rect.height * .5f }, controlExtent,
			controlColor)) {
		index = { EControlIndex::LEFT };
	}
	if (DrawControl({ rect.x, rect.y + rect.height }, controlExtent,
			controlColor)) {
		index = { EControlIndex::BOTTOM | EControlIndex::LEFT };
	}
	if (DrawControl({ rect.x + rect.width, rect.y + rect.height * .5f },
			controlExtent, controlColor)) {
		index = { EControlIndex::RIGHT };
	}
	if (DrawControl({ rect.x + rect.width, rect.y + rect.height },
			controlExtent, controlColor)) {
		index = { EControlIndex::BOTTOM | EControlIndex::RIGHT };
	}
	Color centerColor{ WHITE };
	centerColor.a = 60;
	if (DrawControl({ rect.x + rect.width * .5f,
			  rect.y + rect.height * .5f },
			controlExtent * 2, WHITE)) {
		index = { EControlIndex::CENTER };
	}

	return index;
}
