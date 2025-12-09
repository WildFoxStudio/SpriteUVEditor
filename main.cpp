#ifndef RAYGUI_IMPLEMENTATION
#define RAYGUI_IMPLEMENTATION
#endif
#include "raygui.h"

#include "raylib.h"
#include <nlohmann/json.hpp>

#include "tinyfiledialogs.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <optional>
#include <limits>
#include <iostream>

using json = nlohmann::json;

enum class EAnimationType
{
	SPRITESHEET,
	KEYFRAME,
};

// -------------------------------------------------------------
// Helpers
// -------------------------------------------------------------
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



bool NumericBox(Rectangle rect, char* const name, int strSize, bool& increment, bool& decrement)
{
	increment = false;
	decrement = false;
	// Add plus/minus buttons
	if (GuiButton({ rect.x + rect.width - 30, rect.y, 30, rect.height / 2.f }, "+"))
	{
		increment = true;
	}
	if (GuiButton({ rect.x + rect.width - 30, rect.y + rect.height / 2.f, 30, rect.height / 2.f }, "-"))
	{
		decrement = true;
	}

	if (GuiTextBox({rect.x, rect.y,rect.width - 30, rect.height }, name, strSize, true))
	{
		// Check that there are only numbers
		const std::string_view tmp{ name, (size_t)strSize };
		const bool numericOnly{ std::all_of(tmp.begin(), tmp.end(),[](const char c) {return static_cast<bool>(std::isdigit(c)) || c == '.' || c == ',' || c == '\0'; }) };
		return (numericOnly && strSize);
	}

	return increment || decrement;
}

struct SpritesheetUv
{
	Rectangle Uv{};
	int32_t NumFrames{ 1 };
	int32_t WrapAroundAfter{ std::numeric_limits<int32_t>::max() };
};

std::optional<Texture2D> SpriteTexture{};
std::string ImagePath{};
std::optional<std::string> LastError{};
float gridSize{ 64.f };
std::string GridSizeStr{ std::to_string(gridSize) };

Vector2 pan = { 0, 0 };
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

struct Properties
{
	bool ShowAnimTypeDropDown{};
	int32_t GuiAnimTypeIndex{};
	EAnimationType AnimationType{ EAnimationType::SPRITESHEET };
	int32_t NumOfFrames{};
	std::string NumOfFramesStr{ std::to_string(NumOfFrames) };
	int32_t Columns{};
	std::string ColumnsStr{ std::to_string(Columns) };
};

Rectangle panelView = { 0 };
Vector2 panelScroll = { 0, 0 };

Properties PropertyPanel{};

std::unordered_map<std::string, SpritesheetUv> SpriteNameToUv{};


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
	//    root[s.name] = arr;
	//}

	//// produce <image>.json
	//std::string out = imagePath.substr(0, imagePath.find_last_of(".")) + ".json";
	//std::ofstream of(out);
	//of << root.dump(4);
}

// -------------------------------------------------------------
// Main
// -------------------------------------------------------------

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
		if (!ListState.ShowList)
		{
			float wheel = GetMouseWheelMove();
			if (wheel != 0) {
				Vector2 mouse = GetMousePosition();

				float prevZoom = zoom;
				zoom += wheel * 0.1f;
				if (zoom < 0.1f) zoom = 0.1f;
				if (zoom > 10.0f) zoom = 10.0f;

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

		const int32_t CANVAS_WIDTH{ SpriteTexture.has_value() ? SpriteTexture->width : 0 };
		const int32_t CANVAS_HEIGHT{ SpriteTexture.has_value() ? SpriteTexture->height : 0 };

		const Rectangle canvasRect{ pan.x, pan.y,
				CANVAS_WIDTH * zoom, CANVAS_HEIGHT * zoom };



		Vector2 gridMouseCell = { 0 };
		GuiGrid(canvasRect, "Canvas", gridSize * zoom, 2, &gridMouseCell); // Draw a fancy grid

		// Draw sprite texture
		if (SpriteTexture.has_value()) {
			DrawTextureEx(SpriteTexture.value(), pan, 0, zoom, WHITE);
			// Draw outline
			DrawRectangleLinesEx(canvasRect, 1.f, BLACK);
		}

		// Draw canvas origin axis
		{
			constexpr float AXIS_LEN{ 640000 };
			DrawLineEx(Vector2{ pan.x, pan.y }, Vector2{ pan.x + AXIS_LEN, pan.y }, 2.f, RED);
			DrawLineEx(Vector2{ pan.x, pan.y }, Vector2{ pan.x,pan.y + AXIS_LEN }, 2.f, GREEN);
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

		//// sprite name field
		//GuiTextBox({ 20, 50, 200, 25 }, spriteNameBuf, 64, true);

		//if (GuiButton({ 230, 50, 80, 25 }, "Rename") && selectedSprite >= 0) {
		//	sprites[selectedSprite].name = spriteNameBuf;
		//}

		//// Sprite list
		//for (int i = 0; i < sprites.size(); i++) {
		//	if (GuiButton({ 20, 90.0f + i * 30.0f, 200, 25 }, sprites[i].name.c_str())) {
		//		selectedSprite = i;
		//		selectedFrame = 0;
		//	}
		//}

		// frames for the selected sprite
		//if (selectedSprite >= 0) {
		//	Sprite& s = sprites[selectedSprite];

		//	GuiLabel({ 350, 10, 200, 20 }, ("Frames: " + s.name).c_str());
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
		DrawRectangle(0, 0, GetRenderWidth(), 50, DARKGRAY);

		// Open sprite button
		if (GuiButton({ 10, 10, 200, 30 }, "Open sprite")) {
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
						pan = { 50, 50 };
						zoom = 1.0f;
					}
				}
			}
		}

		// Draw grid size
		{

			GuiDrawRectangle({ 220, 10, 220, 30 }, 1, GRAY, LIGHTGRAY);
			GuiDrawText("Grid size:", Rectangle{ 215, 10, 100, 30 }, 1, DARKGRAY);
			bool inc, dec{};
			if (NumericBox({ 310, 10, 130, 30 }, const_cast<char*>(GridSizeStr.c_str()), GridSizeStr.size(), inc, dec))
			{
				if (inc)
				{
					gridSize = std::max(0.f, gridSize + 1.f);
					GridSizeStr = std::to_string(gridSize);
				}
				else if (dec)
				{
					gridSize = std::max(0.f, gridSize - 1.f);
					GridSizeStr = std::to_string(gridSize);
				}
				else
				{
					gridSize = std::max(0.f, std::stof(GridSizeStr));
				}
				// Restore string
				GridSizeStr = std::to_string(gridSize);
			}

			// Snap to grid
			{
				GuiDrawRectangle({ 450, 10, 150, 30 }, 1, GRAY, LIGHTGRAY);
				GuiCheckBox({ 460, 15, 20, 20 }, "Snap to Grid", &snapToGrid);
			}
		}

		const char* TEMP_ANIM_NAMES[]{ "Animation0" ,"Animation1" ,"Animation2" ,"Animation3" ,"Animation4" ,"Animation5" ,"Animation6" ,"Animation7" ,"Animation8" ,"Animation9" ,"Animation10" };

		// Animation selection
		{
			GuiDrawRectangle({ 610, 10, 310, 30 }, 1, GRAY, LIGHTGRAY);

			if (GuiButton({ 610, 10, 310, 30 }, ListState.activeIndex < 0 ? "No animation" : TEMP_ANIM_NAMES[ListState.activeIndex]))
			{
				ListState.ShowList = !ListState.ShowList;
			}

			if (ListState.ShowList)
			{
				const ListSelection prevState{ ListState };
				GuiScrollPanel({ 620, 40, 330, 300 }, NULL, { 620, 40, 300, 3000 }, &panelScroll, &panelView);
				BeginScissorMode(panelView.x, panelView.y, panelView.width, panelView.height);
				GuiListViewEx({ 620 + panelScroll.x, panelScroll.y + 40, 300, 300 }, TEMP_ANIM_NAMES, 10, &ListState.scrollIndex, &ListState.activeIndex, &ListState.focusIndex);
				EndScissorMode();

				if (ListState.activeIndex != prevState.activeIndex)
				{
					ListState.ShowList = false;
				}
			}
		}

		// Property panel
		{
			constexpr float RIGHTPANELWIDTH{ 380 };
			constexpr float RIGHTPANEL_Y{ 50.f };
			GuiDrawRectangle({ GetRenderWidth() - RIGHTPANELWIDTH, RIGHTPANEL_Y, RIGHTPANELWIDTH, GetRenderHeight() - RIGHTPANEL_Y }, 1, GRAY, DARKGRAY);

			GuiDrawText(ListState.activeIndex < 0 ? "Select an animation" : TEMP_ANIM_NAMES[ListState.activeIndex], { GetRenderWidth() - RIGHTPANELWIDTH, RIGHTPANEL_Y, RIGHTPANELWIDTH, 30 }, TEXT_ALIGN_CENTER, LIGHTGRAY);
			// Draw properties only if selected
			if (ListState.activeIndex > -1)
			{
				// Animation type selection
				{
					if (GuiDropdownBox({ GetRenderWidth() - RIGHTPANELWIDTH, RIGHTPANEL_Y + 30, RIGHTPANELWIDTH, 30 }, "Spritesheet;Keyframe", &PropertyPanel.GuiAnimTypeIndex, PropertyPanel.ShowAnimTypeDropDown))
					{
						PropertyPanel.ShowAnimTypeDropDown = !PropertyPanel.ShowAnimTypeDropDown;

						switch (PropertyPanel.GuiAnimTypeIndex)
						{
						case 0:PropertyPanel.AnimationType = EAnimationType::SPRITESHEET; break;
						case 1:PropertyPanel.AnimationType = EAnimationType::KEYFRAME; break;
						default: break;
						}
					}
				}

				switch (PropertyPanel.AnimationType)
				{
				case EAnimationType::SPRITESHEET:
				{
					bool inc, dec{};
					if (NumericBox({ 310, 10, 130, 30 }, const_cast<char*>(PropertyPanel.NumOfFramesStr.c_str()), PropertyPanel.NumOfFramesStr.size(), inc, dec))
					{
						if (inc)
						{
							PropertyPanel.NumOfFrames = std::max(1, PropertyPanel.NumOfFrames);
							PropertyPanel.NumOfFramesStr = std::to_string(PropertyPanel.NumOfFrames);
						}
						else if (dec)
						{
							PropertyPanel.NumOfFrames = std::max(1, PropertyPanel.NumOfFrames-1);
							PropertyPanel.NumOfFramesStr = std::to_string(PropertyPanel.NumOfFrames);
						}
						else
						{
							PropertyPanel.NumOfFrames = std::max(0, std::stoi(PropertyPanel.NumOfFramesStr));
						}
						// Restore string
						PropertyPanel.NumOfFramesStr = std::to_string(PropertyPanel.NumOfFrames);
					}
				}
				break;

				case EAnimationType::KEYFRAME: {
					DrawText("Not Supported yet!", GetRenderWidth() - RIGHTPANELWIDTH, RIGHTPANEL_Y + 60, GuiGetStyle(DEFAULT, TEXT_SIZE), BLACK);

				} break;

				default: break;
				}



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

#pragma endregion

		//if (IsKeyPressed(KEY_E) && imageLoaded) {
		//	ExportMetadata("spritesheet.png");
		//}

		EndDrawing();
	}

	CloseWindow();
	return 0;
}
