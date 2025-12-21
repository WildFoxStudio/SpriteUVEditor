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

#ifndef RAYGUI_IMPLEMENTATION
#define RAYGUI_IMPLEMENTATION
#endif

#include "raygui.h"
#include "rlgl.h"

#include "definitions.hpp"
#include "app.hpp"
#include "geometry.hpp"
#include "project.hpp"
#include "drawing.hpp"

#include <nlohmann/json.hpp>

#include <unordered_map>
#include <fstream>
#include <optional>
#include <limits>
#include <iostream>
#include <variant>

using json = nlohmann::json;

/* Gui padding*/
constexpr int32_t PAD{ 10 };

/* The loaded sprite sheet texture */
std::optional<Texture2D> SpriteTexture{};

#pragma region Helpers

std::optional<std::string>
LoadSpriteTexture(const std::string &imagePath,
		  std::optional<Texture2D> &outTexture)
{
	Image loadedImg = LoadImage(imagePath.c_str());
	// Failed to open the image!
	if (loadedImg.data == nullptr) {
		return "Failed to open the image!";
	}

	Texture2D newTexture = LoadTextureFromImage(loadedImg);
	UnloadImage(loadedImg);

	// Failed to allocate the sprite GPU texture!
	if (newTexture.id == 0) {
		return "Failed to allocate the sprite GPU texture!";
	}

	// Unload and replace
	if (outTexture.has_value()) {
		UnloadTexture(outTexture.value());
	}
	outTexture.emplace(std::move(newTexture));

	// No error
	return {};
}

float ZoomFitIntoRect(int texWidth, int texHeight, Rectangle targetRect)
{
	const float scaleX{ targetRect.width / static_cast<float>(texWidth) };
	const float scaleY{ targetRect.height / static_cast<float>(texHeight) };
	return std::min(scaleX, scaleY);
}

bool NumericBox(Rectangle rect, char *const name, int *value, int min, int max,
		bool &active, int step = 1)
{
	GuiDrawRectangle(rect, 1, GRAY, LIGHTGRAY);

	const auto textW{ GetTextWidth(name) };
	rect.x += textW;
	rect.width -= textW;

	// Add plus/minus buttons
	if (GuiButton({ rect.x + rect.width - 30, rect.y, 30,
			rect.height / 2.f },
		      "+")) {
		*value = std::min(*value + step, max);
		return true;
	}
	if (GuiButton({ rect.x + rect.width - 30, rect.y + rect.height / 2.f,
			30, rect.height / 2.f },
		      "-")) {
		*value = std::max(*value - step, min);
		return true;
	}
	if (GuiValueBox({ rect.x, rect.y, rect.width - 30, rect.height }, name,
			value, std::min(min, max), std::max(min, max),
			active)) {
		active = !active;
		return true;
	}

	return false;
}

bool StringBox(Rectangle rect, char *const name, int strSize, bool &active)
{
	if (GuiTextBox({ rect.x, rect.y, rect.width, rect.height }, name,
		       strSize, active)) {
		active = !active;
		return true;
	}

	return false;
}

void TextRect(Rectangle rect, const char *const str)
{
	GuiDrawRectangle(rect, 1, GRAY, LIGHTGRAY);
	GuiDrawText(str,
		    { rect.x + 10, rect.y + rect.height * .5f,
		      (float)GetTextWidth(str), 0.f },
		    1, DARKGRAY);
}

template <typename T> void RoundTo(T &value, int grid, bool round)
{
	if (round)
		value = std::round(value / grid) * grid;
}

#pragma endregion Helpers

struct Property {
	int32_t Value{};
	bool ActiveBox{};
};

struct NamedProperty {
	const std::string_view Name;
	Property *const Prop;
};

struct Properties {
	virtual void DrawProperties(Rectangle rect) = 0;
};

struct SpritesheetUv : public Properties {
	Rect Uv{};

	//Properties
	Property Rect[4]{};
	Property AnimTypeIndex{};
	Property NumOfFrames{ 1 };
	Property Columns{ std::numeric_limits<int32_t>::max() };
	Property FrameDurationMs{ 100 };
	bool Looping{ true };

	// Internal data
	Property CurrentFrameIndex{};
	int64_t StartTimeMs{};

	int32_t DraggingControlIndex{};
	Vec2 DeltaMousePos{};

	void DrawProperties(Rectangle rect) override
	{
		rect.height = 30;

		// Draw UV Rect
		{
			Rect[0].Value = Uv.x;
			(void)(NumericBox(rect, "X:", &Rect[0].Value,
					  -INT32_MAX, INT32_MAX,
					  Rect[0].ActiveBox));
			Uv.x = static_cast<float>(Rect[0].Value);
			rect.y += 30 + PAD;
			Rect[1].Value = Uv.y;
			(void)(NumericBox(rect, "Y:", &Rect[1].Value,
					  -INT32_MAX, INT32_MAX,
					  Rect[1].ActiveBox));
			Uv.y = static_cast<float>(Rect[1].Value);
			rect.y += 30 + PAD;
			Rect[2].Value = Uv.w;
			(void)(NumericBox(rect, "Width:", &Rect[2].Value,
					  -INT32_MAX, INT32_MAX,
					  Rect[2].ActiveBox));
			Uv.w = static_cast<float>(Rect[2].Value);
			rect.y += 30 + PAD;
			Rect[3].Value = Uv.h;
			(void)(NumericBox(rect, "Height:", &Rect[3].Value,
					  -INT32_MAX, INT32_MAX,
					  Rect[3].ActiveBox));
			Uv.h = static_cast<float>(Rect[3].Value);
			rect.y += 30 + PAD;
		}

		// Num of frames
		(void)(NumericBox(rect, "Frames:", &NumOfFrames.Value, 1, 8196,
				  NumOfFrames.ActiveBox));

		rect.y += 30 + PAD;

		// Wrap around
		(void)(NumericBox(rect, "Columns:", &Columns.Value, 1, 8196,
				  Columns.ActiveBox));
		// Clamp to at least 1 column
		Columns.Value = std::max(Columns.Value, 1);
		rect.y += 30 + PAD;

		// Frame duration
		(void)(NumericBox(rect,
				  "Frame duration ms:", &FrameDurationMs.Value,
				  0, INT32_MAX, FrameDurationMs.ActiveBox));
		rect.y += 30 + PAD;

		if (SpriteTexture.has_value()) {
			// Draw preview animation frame
			const Rectangle previewRect{ rect.x, rect.y, rect.width,
						     rect.width };

			Rectangle spriteRect{ previewRect };
			// Make the sprite rect fit inside preview rect maintaining aspect ratio
			if (Uv.w > Uv.h) {
				const auto spriteAspectRatio =
					Uv.h / (float)Uv.w;
				spriteRect.width = previewRect.width;
				spriteRect.height =
					previewRect.width * spriteAspectRatio;
			} else {
				const auto spriteAspectRatio =
					Uv.w / (float)Uv.h;
				spriteRect.height = previewRect.height;
				spriteRect.width =
					previewRect.height * spriteAspectRatio;
			}
			// Center the sprite
			{
				spriteRect.x +=
					(previewRect.width - spriteRect.width) *
					.5f;
				spriteRect.y += (previewRect.height -
						 spriteRect.height) *
						.5f;
			}

			// Draw preview background
			DrawRectangleRec(previewRect, WHITE);

			const Vector2 uvOffset{
				(CurrentFrameIndex.Value % Columns.Value) *
					Uv.w,
				(CurrentFrameIndex.Value / Columns.Value) * Uv.h
			};

			const Vector2 uvTopLeft{ Uv.x + uvOffset.x,
						 Uv.y + uvOffset.y };

			const Vector2 uvBottomRight{ uvTopLeft.x + Uv.w,
						     uvTopLeft.y + Uv.h };

			// Draw the UV rect
			rlSetTexture(SpriteTexture->id);
			rlBegin(RL_QUADS);

			rlTexCoord2f(uvTopLeft.x / SpriteTexture->width,
				     uvTopLeft.y / SpriteTexture->height);
			rlVertex2f(spriteRect.x, spriteRect.y);

			rlTexCoord2f(uvTopLeft.x / SpriteTexture->width,
				     uvBottomRight.y / SpriteTexture->height);
			rlVertex2f(spriteRect.x,
				   spriteRect.y + spriteRect.height);

			rlTexCoord2f(uvBottomRight.x / SpriteTexture->width,
				     uvBottomRight.y / SpriteTexture->height);
			rlVertex2f(spriteRect.x + spriteRect.width,
				   spriteRect.y + spriteRect.height);

			rlTexCoord2f(uvBottomRight.x / SpriteTexture->width,
				     uvTopLeft.y / SpriteTexture->height);
			rlVertex2f(spriteRect.x + spriteRect.width,
				   spriteRect.y);

			rlEnd();
			rlSetTexture(0);
		}

		{
			// Advance animation frame
			const int64_t currentTimeMs =
				(int64_t)(GetTime() * 1000.0);
			;
			if (FrameDurationMs.Value > 0 &&
			    NumOfFrames.Value > 1) {
				if (StartTimeMs == 0) {
					StartTimeMs = currentTimeMs;
				}
				const int64_t elapsedMs =
					currentTimeMs - StartTimeMs;
				const int32_t frameAdvances =
					static_cast<int32_t>(
						elapsedMs /
						FrameDurationMs.Value);
				if (frameAdvances > 0) {
					CurrentFrameIndex.Value +=
						frameAdvances;
					if (Looping) {
						CurrentFrameIndex.Value %=
							NumOfFrames.Value;
					} else {
						if (CurrentFrameIndex.Value >=
						    NumOfFrames.Value) {
							CurrentFrameIndex.Value =
								NumOfFrames
									.Value -
								1;
						}
					}
					StartTimeMs += frameAdvances *
						       FrameDurationMs.Value;
				}
			}
		}
	}
};

struct KeyframeUv : public Properties {
	struct Keyframe {
		Rectangle Uv{};
		int32_t FrameDurationMs{ 100 };
	};
	std::vector<Keyframe> Keyframes{};

	void DrawProperties(Rectangle rect) override
	{
		// Draw UV Rect
		constexpr std::string_view err{ "KEYFRAME not Supported yet!" };
		DrawText(err.data(),
			 rect.x + rect.width / 2.f -
				 GetTextWidth(err.data()) / 2.f,
			 rect.y, GuiGetStyle(DEFAULT, TEXT_SIZE), RED);
	}
};

EModalType ActiveModal{ EModalType::NONE };
std::string ImagePath{};
std::optional<std::string> LastError{};
int32_t gridSize{ 64 };
bool gridSizeInputActive{};

View view{};
float fitZoom = 1.0f;

bool drawGrid{ true };
bool snapToGrid{ true };

// Selection list
struct ListSelection {
	int32_t scrollIndex{};
	int32_t activeIndex{};
	int32_t focusIndex{};
	bool ShowList{};
};

ListSelection ListState{};

Rectangle panelView = { 0 };
Vector2 panelScroll = { 0, 0 };

Properties *PropertyPanel{};

using ANIMATION_NAME_T = char[32 + 1];
ANIMATION_NAME_T NewAnimationName{ "Animation_0" };
bool NewAnimationEditMode{ false };
std::unordered_map<std::string, std::variant<SpritesheetUv, KeyframeUv> >
	AnimationNameToSpritesheet{};
std::vector<const char *> ImmutableTransientAnimationNames{};

void RebuildAnimationNamesVector()
{
	ImmutableTransientAnimationNames.clear();
	ImmutableTransientAnimationNames.reserve(
		AnimationNameToSpritesheet.size());
	for (const auto &[name, spriteSheet] : AnimationNameToSpritesheet) {
		ImmutableTransientAnimationNames.push_back(name.data());
	}
}

Rectangle ScreenToImageRect(const Rectangle &r)
{
	if (!SpriteTexture.has_value())
		return {};
	return { (r.x - view.pan.x) / (view.zoom * SpriteTexture->width),
		 (r.y - view.pan.y) / (view.zoom * SpriteTexture->height),
		 r.width / (view.zoom * SpriteTexture->width),
		 r.height / (view.zoom * SpriteTexture->height) };
}

Rectangle ImageToScreenRect(const Rectangle &r)
{
	if (!SpriteTexture.has_value())
		return {};
	return { view.pan.x + r.x * SpriteTexture->width * view.zoom,
		 view.pan.y + r.y * SpriteTexture->height * view.zoom,
		 r.width * SpriteTexture->width * view.zoom,
		 r.height * SpriteTexture->height * view.zoom };
}

void ExportMetadata(const std::string &imagePath)
{
	json root;

	//for (const auto& s : sprites) {
	//    json arr = json::array();
	//    for (const auto& f : s.frames) {
	//        arr.push_back({
	//            {"x", f.uv.x},
	//            {"y", f.uv.y},
	//            {"w", f.uv.width},
	//            {"h", f.uv.height}
	//        });
	//    }
	//    root[s.animationNameOrPlaceholder] = arr;
	//}

	//// produce <image>.json
	//std::string out = imagePath.substr(0, imagePath.find_last_of(".")) + ".json";
	//std::ofstream of(out);
	//of << root.dump(4);
}

int main()
{
	App app(1600, 900, "Sprite Sheet UV Editor");
	if (app.GetFont().texture.id) {
		GuiSetFont(app.GetFont());
	}
	GuiSetStyle(DEFAULT, TEXT_SIZE, 16);

	while (app.ShouldRun()) {
		// -----------------------------------------------------
		// Mouse panning
		// -----------------------------------------------------
		if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
			Vector2 d = GetMouseDelta();
			view.pan.x += d.x;
			view.pan.y += d.y;
		}

		// -----------------------------------------------------
		// Mouse wheel zoom
		// -----------------------------------------------------
		const float prevZoom = view.zoom;
		if (!ListState.ShowList) {
			float wheel = GetMouseWheelMove();
			if (wheel != 0) {
				Vector2 mouse = GetMousePosition();

				// Adjust zoom expotentially -> \frac{x^{2}}{m}\cdot\frac{n}{m}
				view.zoom += std::copysignf(
					(std::pow(wheel * .6f, 2.f) / fitZoom) *
						(view.zoom / fitZoom),
					wheel);
				if (view.zoom < fitZoom / 2)
					view.zoom = fitZoom / 2;
				if (view.zoom > fitZoom * 10)
					view.zoom = fitZoom * 10;

				// zoom to cursor
				view.pan.x = mouse.x -
					     (mouse.x - view.pan.x) *
						     (view.zoom / prevZoom);
				view.pan.y = mouse.y -
					     (mouse.y - view.pan.y) *
						     (view.zoom / prevZoom);
			}
		}

		// -----------------------------------------------------
		// Mouse UV editing (click+drag corners)
		// -----------------------------------------------------
		// if (imageLoaded && selectedSprite >= 0 && selectedFrame >= 0) {
		//     Frame& f = sprites[selectedSprite].frames[selectedFrame];
		//     Rectangle sr = ImageToScreenRect(f.uv);

		//     Vector2 m = GetMousePosition();
		//     const float handle = 8;

		//     bool draggingTL = false, draggingBR = false;
		//     static bool dragTL = false, dragBR = false;

		//     if (CheckCollisionPointRec(m, {sr.x - handle, sr.y - handle, handle*2, handle*2})) {
		//         if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) dragTL = true;
		//     }
		//     if (CheckCollisionPointRec(m, {sr.x + sr.width - handle, sr.y + sr.height - handle, handle*2, handle*2})) {
		//         if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) dragBR = true;
		//     }

		//     if (dragTL && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
		//         Vector2 im = {(m.x - pan.x) / zoom, (m.y - pan.y) / zoom};
		//         f.uv.x = im.x / tex.width;
		//         f.uv.y = im.y / tex.height;
		//         f.uv.width = (sr.x + sr.width - m.x) / (tex.width * zoom);
		//         f.uv.height = (sr.y + sr.height - m.y) / (tex.height * zoom);
		//     }
		//     if (dragBR && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
		//         Vector2 im = {(m.x - pan.x) / zoom, (m.y - pan.y) / zoom};
		//         f.uv.width = im.x / tex.width - f.uv.x;
		//         f.uv.height = im.y / tex.height - f.uv.y;
		//     }

		//     if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
		//         dragTL = dragBR = false;
		//     }
		// }

		// -----------------------------------------------------
		// Draw
		// -----------------------------------------------------
		BeginDrawing();
		ClearBackground(GRAY);

		const bool hasValidSelectedAnimation{
			ListState.activeIndex > -1 &&
			!ImmutableTransientAnimationNames.empty() &&
			ListState.activeIndex <
				ImmutableTransientAnimationNames.size()
		};

		const int32_t CANVAS_WIDTH{ SpriteTexture.has_value() ?
						    SpriteTexture->width :
						    1920 };
		const int32_t CANVAS_HEIGHT{ SpriteTexture.has_value() ?
						     SpriteTexture->height :
						     1080 };

		const Rectangle canvasRect{ view.pan.x, view.pan.y,
					    CANVAS_WIDTH * view.zoom,
					    CANVAS_HEIGHT * view.zoom };

		// Draw sprite texture
		if (SpriteTexture.has_value()) {
			DrawTextureEx(SpriteTexture.value(),
				      to::Vector2_(view.pan), 0, view.zoom,
				      WHITE);
			// Draw outline
			DrawRectangleLinesEx(canvasRect, 1.f, BLACK);
		}

		if (snapToGrid) {
			Vector2 gridMouseCell = { 0 };
			GuiGrid(canvasRect, "Canvas", (gridSize * view.zoom), 1,
				&gridMouseCell); // Draw a fancy grid
		}

		// Draw canvas origin axis
		{
			constexpr int32_t AXIS_LEN{
				std::numeric_limits<int32_t>::max()
			};
			DrawLineEx(to::Vector2_(view.pan),
				   to::Vector2_({ AXIS_LEN,
						  view.pan.y }),
				   2.f, RED);
			DrawLineEx(to::Vector2_(view.pan),
				   to::Vector2_({ view.pan.x,
						  AXIS_LEN }),
				   2.f, GREEN);
		}

		// Draw the selected animation
		if (hasValidSelectedAnimation) {
			PropertyPanel =
				AnimationNameToSpritesheet
							.at(ImmutableTransientAnimationNames
								    [ListState.activeIndex])
							.index() == 0 ?
					reinterpret_cast<
						Properties *>(&std::get<
							      SpritesheetUv>(
						AnimationNameToSpritesheet.at(
							ImmutableTransientAnimationNames
								[ListState
									 .activeIndex]))) :
					reinterpret_cast<
						Properties *>(&std::get<
							      KeyframeUv>(
						AnimationNameToSpritesheet.at(
							ImmutableTransientAnimationNames
								[ListState
									 .activeIndex])));

			auto &animationVariant = AnimationNameToSpritesheet.at(
				ImmutableTransientAnimationNames
					[ListState.activeIndex]);
			if (std::holds_alternative<SpritesheetUv>(
				    animationVariant)) {
				auto &spriteSheet = std::get<SpritesheetUv>(
					animationVariant);

				DrawUVRectDashed(to::Rectangle_(spriteSheet.Uv),
						 view, fitZoom);

				for (int32_t i{ 1 };
				     i < spriteSheet.NumOfFrames.Value; ++i) {
					Rectangle frameUv{ to::Rectangle_(
						spriteSheet.Uv) };
					frameUv.x +=
						(i %
						 spriteSheet.Columns.Value) *
						frameUv.width;
					frameUv.y +=
						(i /
						 spriteSheet.Columns.Value) *
						frameUv.height;
					DrawUVRectDashed(frameUv, view,
							 fitZoom);
				}

				//DrawRectangleRec(spriteSheet.Uv, RED);
				const auto g{ gridSize };

				constexpr float baseControlExtent{ 5.f };
				const auto controlExtent{ baseControlExtent };
				const int32_t focusedControlPoints{
					DrawUvRectControlsGetControlIndex(
						to::Rectangle_(spriteSheet.Uv),
						view, controlExtent)
				};
				if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
					spriteSheet.DraggingControlIndex =
						focusedControlPoints;
				} else if (IsMouseButtonReleased(
						   MOUSE_LEFT_BUTTON) &&
					   spriteSheet.DraggingControlIndex !=
						   EControlIndex::NONE) {
					spriteSheet.DraggingControlIndex =
						EControlIndex::NONE;

					// Normalize sane rectangle always positive values
					if (spriteSheet.Uv.w < 0) {
						spriteSheet.Uv.w *= -1;
						spriteSheet.Uv.x -=
							spriteSheet.Uv.w;
					}
					spriteSheet.Uv.w =
						std::max(snapToGrid ? g : 1,
							 spriteSheet.Uv.w);
					if (spriteSheet.Uv.h < 0) {
						spriteSheet.Uv.h *= -1.f;
						spriteSheet.Uv.y -=
							spriteSheet.Uv.h;
					}
					spriteSheet.Uv.h =
						std::max(snapToGrid ? g : 1,
							 spriteSheet.Uv.h);
				}

				// Get mouse pos in image space
				Vec2 mousePos{};
				{
					auto rayMousePos{ GetMousePosition() };
					rayMousePos.x = (rayMousePos.x *
							 (1.f / view.zoom)) -
							view.pan.x;
					rayMousePos.y = (rayMousePos.y *
							 (1.f / view.zoom)) -
							view.pan.y;
					mousePos = from::Vector2_(rayMousePos);
				}

				//DrawRectanglePro({ 0,0,100 * zoom,100 * zoom }, mousePos, 0.f, RED);

				RoundTo(mousePos.x, g, snapToGrid);
				RoundTo(mousePos.y, g, snapToGrid);
				//printf("Mousepos %f %f\n", mousePos.x, mousePos.y);
				//DrawRectangleRec({ mousePos.x, mousePos.y, 10 * zoom, 10 * zoom }, YELLOW);
				//DrawUVRectDashed({ mousePos.x, mousePos.y, 10, 10 });

				// If zoom has changed
				if (prevZoom != view.zoom) {
					// Reset the delta to avoid unwanted mouse movement
					spriteSheet.DeltaMousePos = mousePos;
				}

				if (spriteSheet.DraggingControlIndex !=
				    EControlIndex::NONE) {
					// Handle dragging
					Vec2 mouseMov{
						spriteSheet.DeltaMousePos.x -
							mousePos.x,
						spriteSheet.DeltaMousePos.y -
							mousePos.y
					};

					if (spriteSheet.DraggingControlIndex &
					    EControlIndex::TOP) {
						auto tempY{ spriteSheet.Uv.y -
							    mouseMov.y };
						RoundTo(tempY, g, snapToGrid);
						const auto movDiff{
							tempY - spriteSheet.Uv.y
						};
						spriteSheet.Uv.y = tempY;
						spriteSheet.Uv.h -=
							std::copysignf(
								movDiff,
								mouseMov.y *
									-1.f);
					}
					if (spriteSheet.DraggingControlIndex &
					    EControlIndex::BOTTOM) {
						spriteSheet.Uv.h -= mouseMov.y;
						RoundTo(spriteSheet.Uv.h, g,
							snapToGrid);
					}
					if (spriteSheet.DraggingControlIndex &
						    EControlIndex::LEFT &&
					    mouseMov.x != 0.f) {
						spriteSheet.Uv.x -= mouseMov.x;
						spriteSheet.Uv.w += mouseMov.x;
						std::cout << "MouseMovX:"
							  << mouseMov.x
							  << " width:"
							  << spriteSheet.Uv.w
							  << std::endl;
						RoundTo(spriteSheet.Uv.x, g,
							snapToGrid);
					}
					if (spriteSheet.DraggingControlIndex &
					    EControlIndex::RIGHT) {
						spriteSheet.Uv.w -= mouseMov.x;
						RoundTo(spriteSheet.Uv.w, g,
							snapToGrid);
					}
				}
				// Update mouse delta at the end
				spriteSheet.DeltaMousePos = mousePos;
			}
		}

#pragma region GUI

		const char *animationNameOrPlaceholder{
			!hasValidSelectedAnimation ?
				"No animation" :
				ImmutableTransientAnimationNames
					[ListState.activeIndex]
		};

		DrawRectangle(0, 0, GetRenderWidth(), 50, DARKGRAY);
		float TITLE_X_OFFSET{ PAD };

		if (ActiveModal != EModalType::NONE) {
			GuiLock();
		}

		// Open sprite button
		const Rectangle openButtonRect{
			TITLE_X_OFFSET, PAD,
			GetTextWidth("Open sprite") * 1.f + PAD, 30
		};
		if (GuiButton(openButtonRect, "Open sprite")) {
			std::string newImagePath{};

			// Since raylib relies on stb_image for image loading right now the formats supported are:
			if (app.OpenFileDialog(newImagePath,
					       { "*.png", "*.jpg", "*.jpeg",
						 "*.bmp", "*.tga", "*.gif" })) {
				std::cout << "Trying to load:" << newImagePath
					  << std::endl;
				// Load texture if possible
				LastError = LoadSpriteTexture(newImagePath,
							      SpriteTexture);
				if (!LastError.has_value()) {
					// Update path
					ImagePath = std::move(newImagePath);

					// Reset view
					{
						view.pan = { 1, PAD * 2 + 30 };
						//Set the zoom to fit the image on the max size
						view.zoom = ZoomFitIntoRect(
							SpriteTexture->width,
							SpriteTexture->height,
							{ 0, 0,
							  GetRenderWidth() -
								  400.f,
							  GetRenderHeight() -
								  100.f });
						fitZoom = view.zoom;
					}
				}
			}
		}
		TITLE_X_OFFSET += openButtonRect.width + PAD;

		// Draw grid size
		{
			const Rectangle rect{ TITLE_X_OFFSET, PAD,
					      GetTextWidth("Grid size") + 80.f,
					      30 };
			(void)(NumericBox(rect, "Grid size", &gridSize, 0, 8196,
					  gridSizeInputActive));

			TITLE_X_OFFSET += rect.width + PAD;

			// Snap to grid
			{
				GuiDrawRectangle({ TITLE_X_OFFSET, PAD, 80,
						   30 },
						 1, GRAY, LIGHTGRAY);
				TITLE_X_OFFSET += PAD / 2;
				GuiCheckBox({ TITLE_X_OFFSET, PAD + 5, 20, 20 },
					    "Snap", &snapToGrid);
			}
			TITLE_X_OFFSET += 80.f;
		}

		{
			// Reset zoom to fit
			const Rectangle fitViewRect{ TITLE_X_OFFSET, PAD,
						     GetTextWidth("Fit view") +
							     PAD,
						     30 };
			if (GuiButton(fitViewRect, "Fit view")) {
				// Reset view
				view.pan = { 1, PAD * 2 + 30 };
				//Set the zoom to fit the image on the max size
				view.zoom = ZoomFitIntoRect(
					CANVAS_WIDTH, CANVAS_HEIGHT,
					{ 0, 0, GetRenderWidth() - 400.f,
					  GetRenderHeight() - 100.f });
			}
			TITLE_X_OFFSET += fitViewRect.width + PAD;
		}

		{
			// Create new animation

			const Rectangle newAnimRect{ TITLE_X_OFFSET, PAD,
						     GetTextWidth("Add") + PAD,
						     30 };
			if (GuiButton(newAnimRect, "Add")) {
				ActiveModal = EModalType::CREATE_ANIMATION;
			}
			TITLE_X_OFFSET += newAnimRect.width + PAD;

			// Delete animation
			if (hasValidSelectedAnimation) {
				const Rectangle delAnimRect{
					TITLE_X_OFFSET, PAD,
					GetTextWidth("Delete") + PAD, 30
				};
				if (GuiButton(delAnimRect, "Delete")) {
					ActiveModal =
						EModalType::CONFIRM_DELETE;
				}
				TITLE_X_OFFSET += delAnimRect.width + PAD;
			}
		}

		// Animation selection
		{
			RebuildAnimationNamesVector();
			const auto nameW{
				std::max(150,
					 GetTextWidth(
						 animationNameOrPlaceholder)) *
				1.f
			};

			const Rectangle animNameRect{ TITLE_X_OFFSET, PAD,
						      nameW, 30 };
			if (GuiButton(animNameRect,
				      animationNameOrPlaceholder)) {
				ListState.ShowList = !ListState.ShowList;
			}
			const auto scrollHeight{ std::clamp(
				ImmutableTransientAnimationNames.size() * 50.f,
				100.f, 500.f) };
			const auto maxStringW{ std::accumulate(
				ImmutableTransientAnimationNames.begin(),
				ImmutableTransientAnimationNames.end(), nameW,
				[](float acc, const char *name) {
					return std::max(
						acc,
						static_cast<float>(
							GetTextWidth(name)));
				}) };

			if (ListState.ShowList) {
				const ListSelection prevState{ ListState };

				const Rectangle panelScrollRect{
					TITLE_X_OFFSET - PAD, PAD + 30,
					maxStringW + PAD * 2, scrollHeight
				};
				const Rectangle animListRect{
					TITLE_X_OFFSET - PAD, PAD + 30,
					maxStringW + PAD * 2, scrollHeight
				};
				GuiScrollPanel(panelScrollRect, NULL,
					       animListRect, &panelScroll,
					       &panelView);
				BeginScissorMode(panelView.x, panelView.y,
						 panelView.width,
						 panelView.height);
				GuiListViewEx(
					animListRect,
					ImmutableTransientAnimationNames.data(),
					ImmutableTransientAnimationNames.size(),
					&ListState.scrollIndex,
					&ListState.activeIndex,
					&ListState.focusIndex);
				// Clamp into range
				ListState.activeIndex = std::clamp(
					ListState.activeIndex, -1,
					static_cast<int32_t>(
						ImmutableTransientAnimationNames
							.size()) -
						1);
				EndScissorMode();

				if (ListState.activeIndex !=
				    prevState.activeIndex) {
					ListState.ShowList = false;
				}
			}
			TITLE_X_OFFSET += animNameRect.width + PAD;

			// Animation type selection
			if (hasValidSelectedAnimation) {
				// TO DO CHOOSE WHEN CREATING ANIMATION ONLY!!!
				//if (GuiDropdownBox({ TITLE_X_OFFSET, PAD, 150, 30 }, "Spritesheet;Keyframe", &PropertyPanel->GuiAnimTypeIndex, PropertyPanel->ShowAnimTypeDropDown))
				//{
				//	PropertyPanel->ShowAnimTypeDropDown = !PropertyPanel->ShowAnimTypeDropDown;

				//	switch (PropertyPanel.GuiAnimTypeIndex)
				//	{
				//	case 0:PropertyPanel.AnimationType = EAnimationType::SPRITESHEET; break;
				//	case 1:PropertyPanel.AnimationType = EAnimationType::KEYFRAME; break;
				//	default: break;
				//	}
				//}
				//TITLE_X_OFFSET += 150 + PAD;
			}
		}

		// Property panel
		{
			constexpr float RIGHTPANEL_W{ 380.f };
			const float RIGHTPANEL_X{ GetRenderWidth() -
						  RIGHTPANEL_W };
			float RIGHTPANEL_Y{ 50.f };
			GuiDrawRectangle({ RIGHTPANEL_X, RIGHTPANEL_Y,
					   RIGHTPANEL_W,
					   GetRenderHeight() - RIGHTPANEL_Y },
					 1, GRAY, DARKGRAY);

			GuiDrawText(animationNameOrPlaceholder,
				    { RIGHTPANEL_X, RIGHTPANEL_Y, RIGHTPANEL_W,
				      30 },
				    TEXT_ALIGN_CENTER, LIGHTGRAY);
			RIGHTPANEL_Y += 30 + PAD;
			// Draw properties only if selected
			if (hasValidSelectedAnimation) {
				PropertyPanel->DrawProperties(
					{ RIGHTPANEL_X + PAD, RIGHTPANEL_Y,
					  RIGHTPANEL_W - PAD * 2.f,
					  GetRenderHeight() - RIGHTPANEL_Y });
			}
		}

		// Draw error string messagebox
		if (LastError.has_value()) {
			//DrawText(LastError->c_str(), 220, 20, 16, RED);
			const auto result = GuiMessageBox(
				Rectangle{ 0, 0, (float)GetRenderWidth(),
					   (float)GetRenderHeight() },
				"Error", LastError->c_str(), "OK");
			if (result > 0) {
				LastError.reset();
			}
		}

		// Draw filename bottom left window
		{
			DrawRectangle(0, GetRenderHeight() - 16,
				      GetRenderWidth(), 16, DARKGRAY);
			DrawText(ImagePath.c_str(), 10, GetRenderHeight() - 16,
				 16, WHITE);
		}

		// Export button
		if (SpriteTexture.has_value() &&
		    GuiButton({ 20, 350, 200, 30 }, "Export JSON")) {
			ExportMetadata("spritesheet.png");
		}

		// Unlock gui
		GuiUnlock();

		// Modals
		{
			Rectangle msgRect{ 0, 0, 600, 300 };
			// Center the rect to the screen
			msgRect.x =
				GetRenderWidth() / 2.f - msgRect.width / 2.f;
			msgRect.y =
				GetRenderHeight() / 2.f - msgRect.height / 2.f;

			if (ActiveModal == EModalType::CREATE_ANIMATION) {
				if (GuiWindowBox(msgRect, "New animation")) {
					ActiveModal = EModalType::NONE;
				}

				// Input box for animationNameOrPlaceholder
				TextRect({ msgRect.x + PAD,
					   msgRect.y + PAD + 30,
					   msgRect.width - PAD * 2, 30 },
					 "Animation name:");
				(void)(StringBox(
					{ msgRect.x + PAD, msgRect.y + PAD + 60,
					  msgRect.width - PAD * 2, 30 },
					NewAnimationName,
					sizeof(NewAnimationName),
					NewAnimationEditMode));
				bool alreadyExists{ false };
				if (const auto found{
					    AnimationNameToSpritesheet.find(
						    NewAnimationName) };
				    found != AnimationNameToSpritesheet.end()) {
					alreadyExists = true;
				}

				if (alreadyExists) {
					DrawText(
						"An animation with this name already exists!",
						msgRect.x + PAD,
						msgRect.y + PAD + 100, 16, RED);
				} else if (!NewAnimationName[0]) {
					DrawText("Must have at least one char!",
						 msgRect.x + PAD,
						 msgRect.y + PAD + 100, 16,
						 RED);
				} else if (GuiButton({ msgRect.x + PAD,
						       msgRect.y +
							       msgRect.height -
							       30 - PAD,
						       100, 30 },
						     "Create")) {
					ActiveModal = EModalType::NONE;
					// Create the animation
					SpritesheetUv spriteSheet{};
					spriteSheet.Uv = { 0, 0, gridSize,
							   gridSize };
					AnimationNameToSpritesheet.emplace(
						std::string(NewAnimationName),
						std::move(spriteSheet));
					ListState.activeIndex =
						AnimationNameToSpritesheet
							.size() -
						1;
				}

				if (GuiButton({ msgRect.x + PAD + 100 + PAD,
						msgRect.y + msgRect.height -
							30 - PAD,
						100, 30 },
					      "Cancel")) {
					ActiveModal = EModalType::NONE;
				}
			} else if (ActiveModal == EModalType::CONFIRM_DELETE) {
				assert(!ImmutableTransientAnimationNames
						.empty());
				std::string tmp{ "Delete " };
				tmp += ImmutableTransientAnimationNames
					[ListState.activeIndex];

				if (GuiMessageBox(msgRect, "Confirm delete",
						  tmp.c_str(),
						  "Cancel;Delete") == 2) {
					ActiveModal = EModalType::NONE;

					AnimationNameToSpritesheet.erase(
						ImmutableTransientAnimationNames
							[ListState.activeIndex]);
					ListState.activeIndex = -1;
					RebuildAnimationNamesVector();
				}
			}
		}

#pragma endregion

		//if (IsKeyPressed(KEY_E) && imageLoaded) {
		//	ExportMetadata("spritesheet.png");
		//}

		EndDrawing();
	}

	return 0;
}
