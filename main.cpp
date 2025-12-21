#ifndef RAYGUI_IMPLEMENTATION
#define RAYGUI_IMPLEMENTATION
#endif
#include "raygui.h"

#include "raylib.h"
#include "rlgl.h"
#include <nlohmann/json.hpp>

#include "tinyfiledialogs.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <optional>
#include <limits>
#include <iostream>
#include <variant>

using json = nlohmann::json;

template<typename T>
struct TVec2 {
	T x{}, y{};
};

template<typename T>
struct TRect {
	T x{}, y{}, w{}, h{};
};


// Use int32_t for all coordinates and sizes since raygui/raylib use int for those
// Also avoid floating point precision issues for pixel coordinates
using Vec2 = TVec2<int32_t>;
using Rect = TRect<int32_t>;

namespace to
{

	Vector2 Vector2_(const Vec2& vec)
	{
		return { static_cast<float>(vec.x), static_cast<float>(vec.y) };
	}

	Rectangle Rectangle_(const Rect& rect)
	{
		return { static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.w), static_cast<float>(rect.h) };
	}


}

namespace from
{
	Vec2 Vector2_(const Vector2& vec)
	{
		return { static_cast<int32_t>(vec.x), static_cast<int32_t>(vec.y) };
	}

	Rectangle Rectangle_(const Rect& rect)
	{
		return { static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.w), static_cast<float>(rect.h) };
	}
}

/* Gui padding*/
constexpr float PAD{ 10.f };

/* The loaded sprite sheet texture */
std::optional<Texture2D> SpriteTexture{};

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
	NONE = 0,
	TOP = 1 << 1,
	BOTTOM = 1 << 2,
	LEFT = 1 << 3,
	RIGHT = 1 << 4,
	CENTER = TOP | BOTTOM | LEFT | RIGHT,
};

#pragma region Helpers
inline bool
OpenFilesDialogSynch(std::string& filePath, const std::vector<std::string>& extension)
{
	std::vector<const char*> extensions;
	extensions.reserve(extension.size());
	std::transform(extension.begin(), extension.end(), std::back_inserter(extensions), [](const std::string& str) { return str.c_str(); });

	const char* result{ tinyfd_openFileDialog("Select a file", NULL, extension.size(), extensions.data(), nullptr, 0) };
	if (result)
	{
		filePath = std::string(result);
		// Tinyfiledialogs not changing the working path to the selected one thus some relative files can not be loaded
		return true;
	}
	return false;
}

std::optional<std::string> LoadSpriteTexture(const std::string& imagePath, std::optional<Texture2D>& outTexture)
{
	Image loadedImg = LoadImage(imagePath.c_str());
	// Failed to open the image!
	if (loadedImg.data == nullptr)
	{
		return "Failed to open the image!";
	}

	Texture2D newTexture = LoadTextureFromImage(loadedImg);
	UnloadImage(loadedImg);

	// Failed to allocate the sprite GPU texture!
	if (newTexture.id == 0)
	{
		return "Failed to allocate the sprite GPU texture!";
	}

	// Unload and replace
	if (outTexture.has_value())
	{
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


bool NumericBox(Rectangle rect, char* const name, int* value, int min, int max, bool& active, int step = 1)
{
	GuiDrawRectangle(rect, 1, GRAY, LIGHTGRAY);

	const auto textW{ GetTextWidth(name) };
	rect.x += textW;
	rect.width -= textW;

	// Add plus/minus buttons
	if (GuiButton({ rect.x + rect.width - 30, rect.y, 30, rect.height / 2.f }, "+"))
	{
		*value = std::min(*value + step, max);
		return true;
	}
	if (GuiButton({ rect.x + rect.width - 30, rect.y + rect.height / 2.f, 30, rect.height / 2.f }, "-"))
	{
		*value = std::max(*value - step, min);
		return true;
	}
	if (GuiValueBox({ rect.x, rect.y,rect.width - 30, rect.height }, name, value, std::min(min, max), std::max(min, max), active))
	{
		active = !active;
		return true;
	}

	return false;
}

bool StringBox(Rectangle rect, char* const name, int strSize, bool& active)
{
	if (GuiTextBox({ rect.x, rect.y,rect.width, rect.height }, name, strSize, active))
	{
		active = !active;
		return true;
	}

	return false;
}

void TextRect(Rectangle rect, const char* const str)
{
	GuiDrawRectangle(rect, 1, GRAY, LIGHTGRAY);
	GuiDrawText(str, { rect.x + 10, rect.y + rect.height * .5f, (float)GetTextWidth(str) ,0.f }, 1, DARKGRAY);
}


template<typename T>
void RoundTo(T& value, int grid, bool round)
{
	if (round)
		value = std::round(value / grid) * grid;
}

#pragma endregion Helpers

struct Property
{
	int32_t Value{};
	bool ActiveBox{};
};

struct NamedProperty
{
	const std::string_view Name;
	Property* const Prop;
};

struct Properties
{
	virtual void DrawProperties(Rectangle rect) = 0;
};

struct SpritesheetUv : public Properties
{
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
			(void)(NumericBox(rect, "X:", &Rect[0].Value, -INT32_MAX, INT32_MAX, Rect[0].ActiveBox));
			Uv.x = static_cast<float>(Rect[0].Value);
			rect.y += 30 + PAD;
			Rect[1].Value = Uv.y;
			(void)(NumericBox(rect, "Y:", &Rect[1].Value, -INT32_MAX, INT32_MAX, Rect[1].ActiveBox));
			Uv.y = static_cast<float>(Rect[1].Value);
			rect.y += 30 + PAD;
			Rect[2].Value = Uv.w;
			(void)(NumericBox(rect, "Width:", &Rect[2].Value, -INT32_MAX, INT32_MAX, Rect[2].ActiveBox));
			Uv.w = static_cast<float>(Rect[2].Value);
			rect.y += 30 + PAD;
			Rect[3].Value = Uv.h;
			(void)(NumericBox(rect, "Height:", &Rect[3].Value, -INT32_MAX, INT32_MAX, Rect[3].ActiveBox));
			Uv.h = static_cast<float>(Rect[3].Value);
			rect.y += 30 + PAD;
		}


		// Num of frames
		(void)(NumericBox(rect, "Frames:", &NumOfFrames.Value, 1, 8196, NumOfFrames.ActiveBox));

		rect.y += 30 + PAD;

		// Wrap around
		(void)(NumericBox(rect, "Columns:", &Columns.Value, 1, 8196, Columns.ActiveBox));
		// Clamp to at least 1 column
		Columns.Value = std::max(Columns.Value, 1);
		rect.y += 30 + PAD;

		// Frame duration
		(void)(NumericBox(rect, "Frame duration ms:", &FrameDurationMs.Value, 0, INT32_MAX, FrameDurationMs.ActiveBox));
		rect.y += 30 + PAD;


		if (SpriteTexture.has_value())
		{
			// Draw preview animation frame
			const Rectangle previewRect{ rect.x, rect.y, rect.width, rect.width };

			Rectangle spriteRect{ previewRect };
			// Make the sprite rect fit inside preview rect maintaining aspect ratio
			if (Uv.w > Uv.h)
			{
				const auto spriteAspectRatio = Uv.h / (float)Uv.w;
				spriteRect.width = previewRect.width;
				spriteRect.height = previewRect.width * spriteAspectRatio;
			}
			else
			{
				const auto spriteAspectRatio = Uv.w / (float)Uv.h;
				spriteRect.height = previewRect.height;
				spriteRect.width = previewRect.height * spriteAspectRatio;
			}
			// Center the sprite
			{
				spriteRect.x += (previewRect.width - spriteRect.width) * .5f;
				spriteRect.y += (previewRect.height - spriteRect.height) * .5f;
			}

			// Draw preview background
			DrawRectangleRec(previewRect, WHITE);


			const Vector2 uvOffset{
				(CurrentFrameIndex.Value % Columns.Value) * Uv.w,
				(CurrentFrameIndex.Value / Columns.Value) * Uv.h
			};

			const Vector2 uvTopLeft{
				Uv.x + uvOffset.x,
				Uv.y + uvOffset.y
			};

			const Vector2 uvBottomRight{
				uvTopLeft.x + Uv.w,
				uvTopLeft.y + Uv.h
			};

			// Draw the UV rect
			rlSetTexture(SpriteTexture->id);
			rlBegin(RL_QUADS);

			rlTexCoord2f(uvTopLeft.x / SpriteTexture->width, uvTopLeft.y / SpriteTexture->height);
			rlVertex2f(spriteRect.x, spriteRect.y);

			rlTexCoord2f(uvTopLeft.x / SpriteTexture->width, uvBottomRight.y / SpriteTexture->height);
			rlVertex2f(spriteRect.x, spriteRect.y + spriteRect.height);


			rlTexCoord2f(uvBottomRight.x / SpriteTexture->width, uvBottomRight.y / SpriteTexture->height);
			rlVertex2f(spriteRect.x + spriteRect.width, spriteRect.y + spriteRect.height);


			rlTexCoord2f(uvBottomRight.x / SpriteTexture->width, uvTopLeft.y / SpriteTexture->height);
			rlVertex2f(spriteRect.x + spriteRect.width, spriteRect.y);


			rlEnd();
			rlSetTexture(0);

		}

		{
			// Advance animation frame
			const int64_t currentTimeMs = (int64_t)(GetTime() * 1000.0);;
			if (FrameDurationMs.Value > 0 && NumOfFrames.Value > 1)
			{
				if (StartTimeMs == 0)
				{
					StartTimeMs = currentTimeMs;
				}
				const int64_t elapsedMs = currentTimeMs - StartTimeMs;
				const int32_t frameAdvances = static_cast<int32_t>(elapsedMs / FrameDurationMs.Value);
				if (frameAdvances > 0)
				{
					CurrentFrameIndex.Value += frameAdvances;
					if (Looping)
					{
						CurrentFrameIndex.Value %= NumOfFrames.Value;
					}
					else
					{
						if (CurrentFrameIndex.Value >= NumOfFrames.Value)
						{
							CurrentFrameIndex.Value = NumOfFrames.Value - 1;
						}
					}
					StartTimeMs += frameAdvances * FrameDurationMs.Value;
				}
			}
		}

	}
};

struct KeyframeUv : public Properties
{
	struct Keyframe
	{
		Rectangle Uv{};
		int32_t FrameDurationMs{ 100 };
	};
	std::vector<Keyframe> Keyframes{};

	void DrawProperties(Rectangle rect) override
	{
		// Draw UV Rect
		constexpr std::string_view err{ "KEYFRAME not Supported yet!" };
		DrawText(err.data(), rect.x + rect.width / 2.f - GetTextWidth(err.data()) / 2.f, rect.y, GuiGetStyle(DEFAULT, TEXT_SIZE), RED);
	}
};



EModalType ActiveModal{ EModalType::NONE };
std::string ImagePath{};
std::optional<std::string> LastError{};
int32_t gridSize{ 64 };
bool gridSizeInputActive{};

Vector2 pan = { 0, 0 };
float fitZoom = 1.0f;
float zoom = 1.0f;
bool drawGrid{ true };
bool snapToGrid{ true };

// Selection list
struct ListSelection
{
	int32_t scrollIndex{};
	int32_t activeIndex{};
	int32_t focusIndex{};
	bool ShowList{};
};

ListSelection ListState{};



Rectangle panelView = { 0 };
Vector2 panelScroll = { 0, 0 };

Properties* PropertyPanel{};

using ANIMATION_NAME_T = char[32 + 1];
ANIMATION_NAME_T NewAnimationName{ "Animation_0" };
bool NewAnimationEditMode{ false };
std::unordered_map<std::string, std::variant<SpritesheetUv, KeyframeUv>> AnimationNameToSpritesheet{};
std::vector<const char*> ImmutableTransientAnimationNames{};

void RebuildAnimationNamesVector()
{
	ImmutableTransientAnimationNames.clear();
	ImmutableTransientAnimationNames.reserve(AnimationNameToSpritesheet.size());
	for (const auto& [name, spriteSheet] : AnimationNameToSpritesheet)
	{
		ImmutableTransientAnimationNames.push_back(name.data());
	}
}


Rectangle ScreenToImageRect(const Rectangle& r) {
	if (!SpriteTexture.has_value())
		return {};
	return {
		(r.x - pan.x) / (zoom * SpriteTexture->width),
		(r.y - pan.y) / (zoom * SpriteTexture->height),
		r.width / (zoom * SpriteTexture->width),
		r.height / (zoom * SpriteTexture->height)
	};
}

Rectangle ImageToScreenRect(const Rectangle& r) {
	if (!SpriteTexture.has_value())
		return {};
	return {
		pan.x + r.x * SpriteTexture->width * zoom,
		pan.y + r.y * SpriteTexture->height * zoom,
		r.width * SpriteTexture->width * zoom,
		r.height * SpriteTexture->height * zoom
	};
}

void DrawDashedLine(Vector2 start, Vector2 end, float dashLength, float gapLength, float thickness, Color color)
{
	float dx = end.x - start.x;
	float dy = end.y - start.y;
	float length = sqrtf(dx * dx + dy * dy);

	float dirX = dx / length;
	float dirY = dy / length;

	float drawn = 0.0f;
	while (drawn < length)
	{
		float segment = fminf(dashLength, length - drawn);

		Vector2 a = { start.x + dirX * drawn, start.y + dirY * drawn };
		Vector2 b = { start.x + dirX * (drawn + segment), start.y + dirY * (drawn + segment) };

		DrawLineEx(a, b, thickness, color);

		drawn += dashLength + gapLength;
	}
}

void DrawUVRectDashed(Rectangle rect)
{

	constexpr Color dashColor{ DARKBLUE };
	rect.x *= zoom;
	rect.y *= zoom;
	rect.x += pan.x;
	rect.y += pan.y;

	rect.width *= zoom;
	rect.height *= zoom;

	constexpr float baseThickness{ 1.8f };
	constexpr float dashLen{ 10 * baseThickness };
	constexpr float dashGap{ 2 * baseThickness };
	const auto thickness{ baseThickness * fitZoom };

	//DrawRectangleLinesEx(rect, 1.f, BLACK);
	DrawDashedLine({ rect.x, rect.y }, { rect.x + rect.width, rect.y }, dashLen, dashGap, thickness, dashColor);
	DrawDashedLine({ rect.x, rect.y + rect.height }, { rect.x + rect.width, rect.y + rect.height }, dashLen, dashGap, thickness, dashColor);

	DrawDashedLine({ rect.x, rect.y }, { rect.x , rect.y + rect.height }, dashLen, dashGap, thickness, dashColor);
	DrawDashedLine({ rect.x + rect.width, rect.y }, { rect.x + rect.width , rect.y + rect.height }, dashLen, dashGap, thickness, dashColor);
}

bool DrawControl(Vector2 origin, float controlExtent, Color baseColor)
{
	constexpr Color focusedColor{ BLUE };
	const Rectangle cRect{ origin.x - controlExtent , origin.y - controlExtent , controlExtent * 2.f , controlExtent * 2.f };

	const bool mouseHover = CheckCollisionPointRec(GetMousePosition(), cRect);

	DrawRectangleRec(cRect, mouseHover ? focusedColor : baseColor);

	return mouseHover;
}

int32_t DrawUvRectControlsGetControlIndex(Rectangle rect, float controlExtent)
{
	constexpr Color controlColor{ DARKBLUE };

	rect.x *= zoom;
	rect.y *= zoom;
	rect.x += pan.x;
	rect.y += pan.y;

	rect.width *= zoom;
	rect.height *= zoom;

	int32_t index{ EControlIndex::NONE };
	if (DrawControl({ rect.x + rect.width * .5f, rect.y }, controlExtent, controlColor))
	{
		index = { EControlIndex::TOP };
	}
	if (DrawControl({ rect.x, rect.y }, controlExtent, controlColor))
	{
		index = { EControlIndex::TOP | EControlIndex::LEFT };
	}
	if (DrawControl({ rect.x + rect.width, rect.y }, controlExtent, controlColor))
	{
		index = { EControlIndex::TOP | EControlIndex::RIGHT };
	}
	if (DrawControl({ rect.x + rect.width * .5f, rect.y + rect.height }, controlExtent, controlColor))
	{
		index = { EControlIndex::BOTTOM };
	}
	if (DrawControl({ rect.x, rect.y + rect.height * .5f }, controlExtent, controlColor))
	{
		index = { EControlIndex::LEFT };
	}
	if (DrawControl({ rect.x, rect.y + rect.height }, controlExtent, controlColor))
	{
		index = { EControlIndex::BOTTOM | EControlIndex::LEFT };
	}
	if (DrawControl({ rect.x + rect.width, rect.y + rect.height * .5f }, controlExtent, controlColor))
	{
		index = { EControlIndex::RIGHT };
	}
	if (DrawControl({ rect.x + rect.width, rect.y + rect.height }, controlExtent, controlColor))
	{
		index = { EControlIndex::BOTTOM | EControlIndex::RIGHT };
	}
	Color centerColor{ WHITE };
	centerColor.a = 60;
	if (DrawControl({ rect.x + rect.width * .5f , rect.y + rect.height * .5f }, controlExtent * 2, WHITE))
	{
		index = { EControlIndex::CENTER };
	}

	return index;
}

void ExportMetadata(const std::string& imagePath) {
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

int main() {
	SetConfigFlags(FLAG_WINDOW_MAXIMIZED | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
	InitWindow(1600, 900, "Sprite Editor (Raylib + Raygui)");

	SetTargetFPS(60);

	GuiSetStyle(DEFAULT, TEXT_SIZE, 18);

	while (!WindowShouldClose()) {
		// -----------------------------------------------------
		// Mouse panning
		// -----------------------------------------------------
		if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
			Vector2 d = GetMouseDelta();
			pan.x += d.x;
			pan.y += d.y;
		}

		// -----------------------------------------------------
		// Mouse wheel zoom
		// -----------------------------------------------------
		const float prevZoom = zoom;
		if (!ListState.ShowList)
		{
			float wheel = GetMouseWheelMove();
			if (wheel != 0) {
				Vector2 mouse = GetMousePosition();

				// Adjust zoom expotentially -> \frac{x^{2}}{m}\cdot\frac{n}{m}
				zoom += std::copysignf((std::pow(wheel * .6f, 2.f) / fitZoom) * (zoom / fitZoom), wheel);
				if (zoom < fitZoom / 2) zoom = fitZoom / 2;
				if (zoom > fitZoom * 10) zoom = fitZoom * 10;

				// zoom to cursor
				pan.x = mouse.x - (mouse.x - pan.x) * (zoom / prevZoom);
				pan.y = mouse.y - (mouse.y - pan.y) * (zoom / prevZoom);
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

		const bool hasValidSelectedAnimation{ ListState.activeIndex > -1 && !ImmutableTransientAnimationNames.empty() && ListState.activeIndex < ImmutableTransientAnimationNames.size() };

		const int32_t CANVAS_WIDTH{ SpriteTexture.has_value() ? SpriteTexture->width : 1920 };
		const int32_t CANVAS_HEIGHT{ SpriteTexture.has_value() ? SpriteTexture->height : 1080 };

		const Rectangle canvasRect{ pan.x, pan.y,
				CANVAS_WIDTH * zoom, CANVAS_HEIGHT * zoom };

		// Draw sprite texture
		if (SpriteTexture.has_value()) {
			DrawTextureEx(SpriteTexture.value(), pan, 0, zoom, WHITE);
			// Draw outline
			DrawRectangleLinesEx(canvasRect, 1.f, BLACK);
		}

		if (snapToGrid)
		{
			Vector2 gridMouseCell = { 0 };
			GuiGrid(canvasRect, "Canvas", (gridSize * zoom), 1, &gridMouseCell); // Draw a fancy grid
		}

		// Draw canvas origin axis
		{
			constexpr float AXIS_LEN{ 640000 };
			DrawLineEx(Vector2{ pan.x, pan.y }, Vector2{ pan.x + AXIS_LEN, pan.y }, 2.f, RED);
			DrawLineEx(Vector2{ pan.x, pan.y }, Vector2{ pan.x,pan.y + AXIS_LEN }, 2.f, GREEN);
		}

		// Draw the selected animation
		if (hasValidSelectedAnimation)
		{
			PropertyPanel = AnimationNameToSpritesheet.at(ImmutableTransientAnimationNames[ListState.activeIndex]).index() == 0 ?
				reinterpret_cast<Properties*>(&std::get<SpritesheetUv>(AnimationNameToSpritesheet.at(ImmutableTransientAnimationNames[ListState.activeIndex]))) :
				reinterpret_cast<Properties*>(&std::get<KeyframeUv>(AnimationNameToSpritesheet.at(ImmutableTransientAnimationNames[ListState.activeIndex])));

			auto& animationVariant = AnimationNameToSpritesheet.at(ImmutableTransientAnimationNames[ListState.activeIndex]);
			if (std::holds_alternative<SpritesheetUv>(animationVariant))
			{
				auto& spriteSheet = std::get<SpritesheetUv>(animationVariant);

				DrawUVRectDashed(to::Rectangle_(spriteSheet.Uv));

				for (int32_t i{ 1 }; i < spriteSheet.NumOfFrames.Value; ++i)
				{
					Rectangle frameUv{ to::Rectangle_(spriteSheet.Uv) };
					frameUv.x += (i % spriteSheet.Columns.Value) * frameUv.width;
					frameUv.y += (i / spriteSheet.Columns.Value) * frameUv.height;
					DrawUVRectDashed(frameUv);
				}

				//DrawRectangleRec(spriteSheet.Uv, RED);
				const auto g{ gridSize };

				constexpr float baseControlExtent{ 5.f };
				const auto controlExtent{ baseControlExtent };
				const int32_t focusedControlPoints{ DrawUvRectControlsGetControlIndex(to::Rectangle_(spriteSheet.Uv), controlExtent) };
				if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
				{
					spriteSheet.DraggingControlIndex = focusedControlPoints;
				}
				else
					if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && spriteSheet.DraggingControlIndex != EControlIndex::NONE)
					{
						spriteSheet.DraggingControlIndex = EControlIndex::NONE;

						// Normalize sane rectangle always positive values
						if (spriteSheet.Uv.w < 0)
						{
							spriteSheet.Uv.w *= -1;
							spriteSheet.Uv.x -= spriteSheet.Uv.w;
						}
						spriteSheet.Uv.w = std::max(snapToGrid ? g : 1, spriteSheet.Uv.w);
						if (spriteSheet.Uv.h < 0)
						{
							spriteSheet.Uv.h *= -1.f;
							spriteSheet.Uv.y -= spriteSheet.Uv.h;
						}
						spriteSheet.Uv.h = std::max(snapToGrid ? g : 1, spriteSheet.Uv.h);
					}

				// Get mouse pos in image space
				Vec2 mousePos{};
				{
					auto rayMousePos{ GetMousePosition() };
					rayMousePos.x = (rayMousePos.x * (1.f / zoom)) - pan.x;
					rayMousePos.y = (rayMousePos.y * (1.f / zoom)) - pan.y;
					mousePos = from::Vector2_(rayMousePos);
				}

				//DrawRectanglePro({ 0,0,100 * zoom,100 * zoom }, mousePos, 0.f, RED);

				RoundTo(mousePos.x, g, snapToGrid);
				RoundTo(mousePos.y, g, snapToGrid);
				//printf("Mousepos %f %f\n", mousePos.x, mousePos.y);
				//DrawRectangleRec({ mousePos.x, mousePos.y, 10 * zoom, 10 * zoom }, YELLOW);
				//DrawUVRectDashed({ mousePos.x, mousePos.y, 10, 10 });

				// If zoom has changed
				if (prevZoom != zoom)
				{
					// Reset the delta to avoid unwanted mouse movement
					spriteSheet.DeltaMousePos = mousePos;
				}

				if (spriteSheet.DraggingControlIndex != EControlIndex::NONE)
				{

					// Handle dragging
					Vec2 mouseMov{ spriteSheet.DeltaMousePos.x - mousePos.x, spriteSheet.DeltaMousePos.y - mousePos.y };

					if (spriteSheet.DraggingControlIndex & EControlIndex::TOP)
					{
						auto tempY{ spriteSheet.Uv.y - mouseMov.y };
						RoundTo(tempY, g, snapToGrid);
						const auto movDiff{ tempY - spriteSheet.Uv.y };
						spriteSheet.Uv.y = tempY;
						spriteSheet.Uv.h -= std::copysignf(movDiff, mouseMov.y * -1.f);
					}
					if (spriteSheet.DraggingControlIndex & EControlIndex::BOTTOM)
					{
						spriteSheet.Uv.h -= mouseMov.y;
						RoundTo(spriteSheet.Uv.h, g, snapToGrid);
					}
					if (spriteSheet.DraggingControlIndex & EControlIndex::LEFT && mouseMov.x != 0.f)
					{
						spriteSheet.Uv.x -= mouseMov.x;
						spriteSheet.Uv.w += mouseMov.x;
						std::cout << "MouseMovX:" << mouseMov.x << " width:" << spriteSheet.Uv.w << std::endl;
						RoundTo(spriteSheet.Uv.x, g, snapToGrid);
					}
					if (spriteSheet.DraggingControlIndex & EControlIndex::RIGHT)
					{
						spriteSheet.Uv.w -= mouseMov.x;
						RoundTo(spriteSheet.Uv.w, g, snapToGrid);
					}


				}
				// Update mouse delta at the end
				spriteSheet.DeltaMousePos = mousePos;
			}
		}

		// Draw sprite UV previews
		//if (imageLoaded) {
		//	for (int i = 0; i < sprites.size(); i++) {
		//		for (int j = 0; j < sprites[i].frames.size(); j++) {
		//			Rectangle r = ImageToScreenRect(sprites[i].frames[j].uv);

		//			Color col = (i == selectedSprite && j == selectedFrame)
		//				? YELLOW : Fade(RED, 0.7);

		//			DrawRectangleLinesEx(r, 2, col);
		//		}
		//	}
		//}

		//// GUI panel
		//GuiPanel({ 10, 10, 320, 380 }, "Sprites");

		//// sprite animationNameOrPlaceholder field
		//GuiTextBox({ 20, 50, 200, 25 }, spriteNameBuf, 64, true);

		//if (GuiButton({ 230, 50, 80, 25 }, "Rename") && selectedSprite >= 0) {
		//	sprites[selectedSprite].animationNameOrPlaceholder = spriteNameBuf;
		//}

		//// Sprite list
		//for (int i = 0; i < sprites.size(); i++) {
		//	if (GuiButton({ 20, 90.0f + i * 30.0f, 200, 25 }, sprites[i].animationNameOrPlaceholder.c_str())) {
		//		selectedSprite = i;
		//		selectedFrame = 0;
		//	}
		//}

		// frames for the selected sprite
		//if (selectedSprite >= 0) {
		//	Sprite& s = sprites[selectedSprite];

		//	GuiLabel({ 350, 10, 200, 20 }, ("Frames: " + s.animationNameOrPlaceholder).c_str());
		//	for (int j = 0; j < s.frames.size(); j++) {
		//		if (GuiButton({ 350, 40.0f + j * 30.0f, 150, 25 }, ("Frame " + std::to_string(j)).c_str())) {
		//			selectedFrame = j;
		//		}
		//	}

		//	if (GuiButton({ 350, 40.0f + static_cast<float>(s.frames.size()) * 30.0f, 150, 25 }, "Add Frame")) {
		//		s.frames.push_back(s.frames.back());
		//		selectedFrame = (int)s.frames.size() - 1;
		//	}
		//}
#pragma region GUI

		const char* animationNameOrPlaceholder{ !hasValidSelectedAnimation ? "No animation" : ImmutableTransientAnimationNames[ListState.activeIndex] };

		DrawRectangle(0, 0, GetRenderWidth(), 50, DARKGRAY);
		float TITLE_X_OFFSET{ PAD };

		if (ActiveModal != EModalType::NONE)
		{
			GuiLock();
		}

		// Open sprite button
		const Rectangle openButtonRect{ TITLE_X_OFFSET, PAD, GetTextWidth("Open sprite") * 1.f + PAD, 30 };
		if (GuiButton(openButtonRect, "Open sprite")) {
			std::string newImagePath{};
			if (OpenFilesDialogSynch(newImagePath, { "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tga", "*.dds", "*.ktx", "*.pkm", "*.pvr", "*.astc" })) {
				std::cout << "Trying to load:" << newImagePath << std::endl;
				// Load texture is possible
				LastError = LoadSpriteTexture(newImagePath, SpriteTexture);
				if (!LastError.has_value())
				{
					// Update path
					ImagePath = std::move(newImagePath);

					// Reset view
					{
						pan = { 1, PAD * 2 + 30 };
						//Set the zoom to fit the image on the max size
						zoom = ZoomFitIntoRect(SpriteTexture->width, SpriteTexture->height, { 0, 0, GetRenderWidth() - 400.f, GetRenderHeight() - 100.f });
						fitZoom = zoom;
					}
				}
			}
		}
		TITLE_X_OFFSET += openButtonRect.width + PAD;

		// Draw grid size
		{
			const Rectangle rect{ TITLE_X_OFFSET, PAD,GetTextWidth("Grid size") + 80.f, 30 };
			(void)(NumericBox(rect, "Grid size", &gridSize, 0, 8196, gridSizeInputActive));

			TITLE_X_OFFSET += rect.width + PAD;

			// Snap to grid
			{
				GuiDrawRectangle({ TITLE_X_OFFSET, PAD, 80, 30 }, 1, GRAY, LIGHTGRAY);
				TITLE_X_OFFSET += PAD / 2;
				GuiCheckBox({ TITLE_X_OFFSET, PAD + 5, 20, 20 }, "Snap", &snapToGrid);
			}
			TITLE_X_OFFSET += 80.f;
		}

		{
			// Reset zoom to fit
			const Rectangle fitViewRect{ TITLE_X_OFFSET, PAD, GetTextWidth("Fit view") + PAD, 30 };
			if (GuiButton(fitViewRect, "Fit view"))
			{
				// Reset view
				pan = { 1, PAD * 2 + 30 };
				//Set the zoom to fit the image on the max size
				zoom = ZoomFitIntoRect(CANVAS_WIDTH, CANVAS_HEIGHT, { 0, 0, GetRenderWidth() - 400.f, GetRenderHeight() - 100.f });
			}
			TITLE_X_OFFSET += fitViewRect.width + PAD;
		}

		{
			// Create new animation

			const Rectangle newAnimRect{ TITLE_X_OFFSET, PAD, GetTextWidth("Add") + PAD, 30 };
			if (GuiButton(newAnimRect, "Add"))
			{
				ActiveModal = EModalType::CREATE_ANIMATION;
			}
			TITLE_X_OFFSET += newAnimRect.width + PAD;

			// Delete animation
			if (hasValidSelectedAnimation)
			{
				const Rectangle delAnimRect{ TITLE_X_OFFSET, PAD, GetTextWidth("Delete") + PAD, 30 };
				if (GuiButton(delAnimRect, "Delete"))
				{
					ActiveModal = EModalType::CONFIRM_DELETE;
				}
				TITLE_X_OFFSET += delAnimRect.width + PAD;
			}
		}



		// Animation selection
		{
			RebuildAnimationNamesVector();
			const auto nameW{ std::max(150,GetTextWidth(animationNameOrPlaceholder)) * 1.f };

			const Rectangle animNameRect{ TITLE_X_OFFSET, PAD, nameW, 30 };
			if (GuiButton(animNameRect, animationNameOrPlaceholder))
			{
				ListState.ShowList = !ListState.ShowList;
			}
			const auto scrollHeight{ std::clamp(ImmutableTransientAnimationNames.size() * 50.f, 100.f, 500.f) };
			const auto maxStringW{ std::accumulate(ImmutableTransientAnimationNames.begin(), ImmutableTransientAnimationNames.end(), nameW,
				[](float acc, const char* name) {
					return std::max(acc, static_cast<float>(GetTextWidth(name)));
				}) };

			if (ListState.ShowList)
			{
				const ListSelection prevState{ ListState };

				const Rectangle panelScrollRect{ TITLE_X_OFFSET - PAD, PAD + 30, maxStringW + PAD * 2 ,scrollHeight };
				const Rectangle animListRect{ TITLE_X_OFFSET - PAD, PAD + 30, maxStringW + PAD * 2 ,scrollHeight };
				GuiScrollPanel(panelScrollRect, NULL, animListRect, &panelScroll, &panelView);
				BeginScissorMode(panelView.x, panelView.y, panelView.width, panelView.height);
				GuiListViewEx(animListRect, ImmutableTransientAnimationNames.data(), ImmutableTransientAnimationNames.size(), &ListState.scrollIndex, &ListState.activeIndex, &ListState.focusIndex);
				// Clamp into range
				ListState.activeIndex = std::clamp(ListState.activeIndex, -1, static_cast<int32_t>(ImmutableTransientAnimationNames.size()) - 1);
				EndScissorMode();

				if (ListState.activeIndex != prevState.activeIndex)
				{
					ListState.ShowList = false;
				}
			}
			TITLE_X_OFFSET += animNameRect.width + PAD;

			// Animation type selection
			if (hasValidSelectedAnimation)
			{
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
			const float RIGHTPANEL_X{ GetRenderWidth() - RIGHTPANEL_W };
			float RIGHTPANEL_Y{ 50.f };
			GuiDrawRectangle({ RIGHTPANEL_X, RIGHTPANEL_Y, RIGHTPANEL_W, GetRenderHeight() - RIGHTPANEL_Y }, 1, GRAY, DARKGRAY);

			GuiDrawText(animationNameOrPlaceholder, { RIGHTPANEL_X, RIGHTPANEL_Y, RIGHTPANEL_W, 30 }, TEXT_ALIGN_CENTER, LIGHTGRAY);
			RIGHTPANEL_Y += 30 + PAD;
			// Draw properties only if selected
			if (hasValidSelectedAnimation)
			{
				PropertyPanel->DrawProperties({ RIGHTPANEL_X + PAD, RIGHTPANEL_Y, RIGHTPANEL_W - PAD * 2.f, GetRenderHeight() - RIGHTPANEL_Y });
			}
		}




		// Draw error string messagebox
		if (LastError.has_value())
		{
			//DrawText(LastError->c_str(), 220, 20, 16, RED);
			const auto result = GuiMessageBox(Rectangle{ 0,0, (float)GetRenderWidth(), (float)GetRenderHeight() }, "Error", LastError->c_str(), "OK");
			if (result > 0)
			{
				LastError.reset();
			}
		}

		// Draw filename bottom left window
		{

			DrawRectangle(0, GetRenderHeight() - 16, GetRenderWidth(), 16, DARKGRAY);
			DrawText(ImagePath.c_str(), 10, GetRenderHeight() - 16, 16, WHITE);
		}


		// Export button
		if (SpriteTexture.has_value() && GuiButton({ 20, 350, 200, 30 }, "Export JSON")) {
			ExportMetadata("spritesheet.png");
		}


		// Unlock gui
		GuiUnlock();

		// Modals
		{
			Rectangle msgRect{ 0,0,600,300 };
			// Center the rect to the screen
			msgRect.x = GetRenderWidth() / 2.f - msgRect.width / 2.f;
			msgRect.y = GetRenderHeight() / 2.f - msgRect.height / 2.f;

			if (ActiveModal == EModalType::CREATE_ANIMATION)
			{


				if (GuiWindowBox(msgRect, "New animation"))
				{
					ActiveModal = EModalType::NONE;
				}

				// Input box for animationNameOrPlaceholder
				TextRect({ msgRect.x + PAD, msgRect.y + PAD + 30, msgRect.width - PAD * 2, 30 }, "Animation name:");
				(void)(StringBox({ msgRect.x + PAD, msgRect.y + PAD + 60, msgRect.width - PAD * 2, 30 }, NewAnimationName, sizeof(NewAnimationName), NewAnimationEditMode));
				bool alreadyExists{ false };
				if (const auto found{ AnimationNameToSpritesheet.find(NewAnimationName) }; found != AnimationNameToSpritesheet.end())
				{
					alreadyExists = true;
				}

				if (alreadyExists)
				{
					DrawText("An animation with this name already exists!", msgRect.x + PAD, msgRect.y + PAD + 100, 16, RED);
				}
				else if (!NewAnimationName[0])
				{
					DrawText("Must have at least one char!", msgRect.x + PAD, msgRect.y + PAD + 100, 16, RED);
				}
				else
					if (GuiButton({ msgRect.x + PAD, msgRect.y + msgRect.height - 30 - PAD, 100, 30 }, "Create"))
					{
						ActiveModal = EModalType::NONE;
						// Create the animation
						SpritesheetUv spriteSheet{};
						spriteSheet.Uv = { 0,0, gridSize, gridSize };
						AnimationNameToSpritesheet.emplace(std::string(NewAnimationName), std::move(spriteSheet));
						ListState.activeIndex = AnimationNameToSpritesheet.size() - 1;
					}

				if (GuiButton({ msgRect.x + PAD + 100 + PAD, msgRect.y + msgRect.height - 30 - PAD, 100, 30 }, "Cancel"))
				{
					ActiveModal = EModalType::NONE;
				}
			}
			else if (ActiveModal == EModalType::CONFIRM_DELETE)
			{
				assert(!ImmutableTransientAnimationNames.empty());
				std::string tmp{ "Delete " };
				tmp += ImmutableTransientAnimationNames[ListState.activeIndex];

				if (GuiMessageBox(msgRect, "Confirm delete", tmp.c_str(), "Cancel;Delete") == 2)
				{
					ActiveModal = EModalType::NONE;

					AnimationNameToSpritesheet.erase(ImmutableTransientAnimationNames[ListState.activeIndex]);
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

	CloseWindow();
	return 0;
}
