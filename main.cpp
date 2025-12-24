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

#include "app.hpp"
#include "definitions.hpp"
#include "drawing.hpp"
#include "geometry.hpp"
#include "project.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <unordered_map>
#include <variant>

/**
 * \brief The GUI padding between elements.
 */
constexpr int32_t PAD{ 10 };
constexpr float   ZOOM_STEP{ 0.12f };
constexpr int32_t VIEWPORT_GUI_RIGHT_PANEL_WIDTH{ 400 };
constexpr int32_t VIEWPORT_GUI_OCCLUSION_Y{ 100 };
constexpr int32_t DEFAULT_CANVAS_WIDTH{ 1920 };
constexpr int32_t DEFAULT_CANVAS_HEIGHT{ 1080 };
/**
 * \brief Currently active modal dialog.
 */
EModalType ActiveModal{ EModalType::NONE };
/**
 * \brief The camera/view states.
 */
View defaultView{};
View view{};
/**
 * \brief Current loaded project - must always be valid ptr.
 */
std::unique_ptr<Project> CP{ std::make_unique<Project>() };

#pragma region Helpers
void
ResetViewToDefault()
{
    view = defaultView;
}

float
GetStringWidth(const std::string& str)
{
    return static_cast<float>(GetTextWidth(str.c_str()) + PAD);
}

bool
NumericBox(Rectangle rect, char* const name, int* value, int min, int max, bool& active, int step = 1)
{
    GuiDrawRectangle(rect, 1, GRAY, LIGHTGRAY);

    const auto textW{ GetStringWidth(name) };
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
    if (GuiValueBox({ rect.x, rect.y, rect.width - 30, rect.height }, name, value, std::min(min, max), std::max(min, max), active))
        {
            active = !active;
            return true;
        }

    return false;
}

bool
StringBox(Rectangle rect, char* const name, int strSize, bool& active)
{
    if (GuiTextBox({ rect.x, rect.y, rect.width, rect.height }, name, strSize, active))
        {
            active = !active;
            return true;
        }

    return false;
}

void
TextRect(Rectangle rect, const char* const str)
{
    GuiDrawRectangle(rect, 1, GRAY, LIGHTGRAY);
    GuiDrawText(str, { rect.x + 10, rect.y + rect.height * .5f, GetStringWidth(str), 0.f }, 1, DARKGRAY);
}

template<typename T>
void
RoundTo(T& value, int grid, bool round)
{
    if (round)
        value = std::round(value / grid) * grid;
}

#pragma endregion Helpers

void
DrawSpritesheetUvProperties(Rectangle rect, SpritesheetUv& p)
{
    rect.height = 30;

    // Draw UV Rect
    {
        p.Property_Rect[0].Value = p.Uv.x;
        (void)(NumericBox(rect, "X:", &p.Property_Rect[0].Value, -INT32_MAX, INT32_MAX, p.Property_Rect[0].ActiveBox));
        p.Uv.x = static_cast<float>(p.Property_Rect[0].Value);
        rect.y += 30 + PAD;
        p.Property_Rect[1].Value = p.Uv.y;
        (void)(NumericBox(rect, "Y:", &p.Property_Rect[1].Value, -INT32_MAX, INT32_MAX, p.Property_Rect[1].ActiveBox));
        p.Uv.y = static_cast<float>(p.Property_Rect[1].Value);
        rect.y += 30 + PAD;
        p.Property_Rect[2].Value = p.Uv.w;
        (void)(NumericBox(rect, "Width:", &p.Property_Rect[2].Value, -INT32_MAX, INT32_MAX, p.Property_Rect[2].ActiveBox));
        p.Uv.w = static_cast<float>(p.Property_Rect[2].Value);
        rect.y += 30 + PAD;
        p.Property_Rect[3].Value = p.Uv.h;
        (void)(NumericBox(rect, "Height:", &p.Property_Rect[3].Value, -INT32_MAX, INT32_MAX, p.Property_Rect[3].ActiveBox));
        p.Uv.h = static_cast<float>(p.Property_Rect[3].Value);
        rect.y += 30 + PAD;
    }

    // Num of frames
    (void)(NumericBox(rect, "Frames:", &p.Property_NumOfFrames.Value, 1, 8196, p.Property_NumOfFrames.ActiveBox));

    rect.y += 30 + PAD;

    // Wrap around
    (void)(NumericBox(rect, "Columns:", &p.Property_Columns.Value, 1, 8196, p.Property_Columns.ActiveBox));
    // Clamp to at least 1 column
    p.Property_Columns.Value = std::max(p.Property_Columns.Value, 1);
    rect.y += 30 + PAD;

    // Frame duration
    (void)(NumericBox(rect, "Frame duration ms:", &p.Property_FrameDurationMs.Value, 0, INT32_MAX, p.Property_FrameDurationMs.ActiveBox));
    rect.y += 30 + PAD;

    if (CP->SpriteTexture.has_value())
        {
            // Draw preview animation frame
            const Rectangle previewRect{ rect.x, rect.y, rect.width, rect.width };

            Rectangle spriteRect{ previewRect };
            // Make the sprite rect fit inside preview rect maintaining aspect ratio
            if (p.Uv.w > p.Uv.h)
                {
                    const auto spriteAspectRatio = p.Uv.h / (float)p.Uv.w;
                    spriteRect.width             = previewRect.width;
                    spriteRect.height            = previewRect.width * spriteAspectRatio;
                }
            else
                {
                    const auto spriteAspectRatio = p.Uv.w / (float)p.Uv.h;
                    spriteRect.height            = previewRect.height;
                    spriteRect.width             = previewRect.height * spriteAspectRatio;
                }
            // Center the sprite
            {
                spriteRect.x += (previewRect.width - spriteRect.width) * .5f;
                spriteRect.y += (previewRect.height - spriteRect.height) * .5f;
            }

            // Draw preview background
            DrawRectangleRec(previewRect, WHITE);

            const Vec2 uvOffset{ (p.CurrentFrameIndex.Value % p.Property_Columns.Value) * p.Uv.w, (p.CurrentFrameIndex.Value / p.Property_Columns.Value) * p.Uv.h };

            const Vec2 uvTopLeft{ p.Uv.x + uvOffset.x, p.Uv.y + uvOffset.y };

            const Vec2 uvBottomRight{ uvTopLeft.x + p.Uv.w, uvTopLeft.y + p.Uv.h };

            // Draw the UV rect
            rlSetTexture(CP->SpriteTexture->id);
            rlBegin(RL_QUADS);

            rlTexCoord2f(uvTopLeft.x / CP->SpriteTexture->width, uvTopLeft.y / CP->SpriteTexture->height);
            rlVertex2f(spriteRect.x, spriteRect.y);

            rlTexCoord2f(uvTopLeft.x / CP->SpriteTexture->width, uvBottomRight.y / CP->SpriteTexture->height);
            rlVertex2f(spriteRect.x, spriteRect.y + spriteRect.height);

            rlTexCoord2f(uvBottomRight.x / CP->SpriteTexture->width, uvBottomRight.y / CP->SpriteTexture->height);
            rlVertex2f(spriteRect.x + spriteRect.width, spriteRect.y + spriteRect.height);

            rlTexCoord2f(uvBottomRight.x / CP->SpriteTexture->width, uvTopLeft.y / CP->SpriteTexture->height);
            rlVertex2f(spriteRect.x + spriteRect.width, spriteRect.y);

            rlEnd();
            rlSetTexture(0);
        }

    {
        // Advance animation frame
        const int64_t currentTimeMs = (int64_t)(GetTime() * 1000.0);
        ;
        if (p.Property_FrameDurationMs.Value > 0 && p.Property_NumOfFrames.Value > 1)
            {
                if (p.StartTimeMs == 0)
                    {
                        p.StartTimeMs = currentTimeMs;
                    }
                const int64_t elapsedMs     = currentTimeMs - p.StartTimeMs;
                const int32_t frameAdvances = static_cast<int32_t>(elapsedMs / p.Property_FrameDurationMs.Value);
                if (frameAdvances > 0)
                    {
                        p.CurrentFrameIndex.Value += frameAdvances;
                        if (p.Looping)
                            {
                                p.CurrentFrameIndex.Value %= p.Property_NumOfFrames.Value;
                            }
                        else
                            {
                                if (p.CurrentFrameIndex.Value >= p.Property_NumOfFrames.Value)
                                    {
                                        p.CurrentFrameIndex.Value = p.Property_NumOfFrames.Value - 1;
                                    }
                            }
                        p.StartTimeMs += frameAdvances * p.Property_FrameDurationMs.Value;
                    }
            }
    }
}

void
DrawKeyframeProperties(Rectangle rect, KeyframeUv& p)
{
    // Draw UV Rect
    constexpr std::string_view err{ "KEYFRAME not Supported yet!" };
    DrawText(err.data(), rect.x + rect.width / 2.f - GetStringWidth(err.data()) / 2.f, rect.y, GuiGetStyle(DEFAULT, TEXT_SIZE), RED);
}

void
DrawPropertiesIfValidPtr(Rectangle rect, AnimationData* p)
{
    if (!p)
        {
            return;
        }

    if (std::holds_alternative<SpritesheetUv>(p->Data))
        {
            DrawSpritesheetUvProperties(rect, std::get<SpritesheetUv>(p->Data));
        }
    else if (std::holds_alternative<KeyframeUv>(p->Data))
        {
            DrawKeyframeProperties(rect, std::get<KeyframeUv>(p->Data));
        }
    else
        {
            assert(false && "Unknown property variant!");
        }
}

// Selection list
struct ListSelection
{
    int32_t scrollIndex{ -1 };
    int32_t activeIndex{ -1 };
    int32_t focusIndex{ -1 };
    bool    ShowList{};
};

ListSelection ListState{};

Rectangle panelView   = { 0 };
Vector2   panelScroll = { 0, 0 };

using ANIMATION_NAME_T = char[32 + 1];
ANIMATION_NAME_T NewAnimationName{ "Animation_0" };
bool             NewAnimationEditMode{ false };

Rectangle
ScreenToImageRect(const Rectangle& r)
{
    if (!CP->SpriteTexture.has_value())
        return {};
    return { (r.x - view.pan.x) / (view.zoom * CP->SpriteTexture->width),
        (r.y - view.pan.y) / (view.zoom * CP->SpriteTexture->height),
        r.width / (view.zoom * CP->SpriteTexture->width),
        r.height / (view.zoom * CP->SpriteTexture->height) };
}

Rectangle
ImageToScreenRect(const Rectangle& r)
{
    if (!CP->SpriteTexture.has_value())
        return {};
    return { view.pan.x + r.x * CP->SpriteTexture->width * view.zoom,
        view.pan.y + r.y * CP->SpriteTexture->height * view.zoom,
        r.width * CP->SpriteTexture->width * view.zoom,
        r.height * CP->SpriteTexture->height * view.zoom };
}

int
main()
{
    App app(1600, 900, "Sprite Sheet UV Editor");
    if (app.GetFont().texture.id)
        {
            GuiSetFont(app.GetFont());
        }
    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);

    // Set the zoom to fit the image on the max size
    {
        defaultView.pan = { 1, PAD * 2 + 30 };
        defaultView.fitZoom =
        View::ZoomFitIntoRect(DEFAULT_CANVAS_WIDTH, DEFAULT_CANVAS_HEIGHT, { 0, 0, GetRenderWidth() - VIEWPORT_GUI_RIGHT_PANEL_WIDTH, GetRenderHeight() - VIEWPORT_GUI_OCCLUSION_Y });
        defaultView.zoom = view.fitZoom;
        assert(defaultView.fitZoom > 0.f);
    }

    while (app.ShouldRun())
        {

            const int32_t CANVAS_WIDTH{ CP->SpriteTexture.has_value() ? CP->SpriteTexture->width : DEFAULT_CANVAS_WIDTH };
            const int32_t CANVAS_HEIGHT{ CP->SpriteTexture.has_value() ? CP->SpriteTexture->height : DEFAULT_CANVAS_HEIGHT };

#pragma region Events
            {
                // Handle window resize viewport
                if (IsWindowResized())
                    {
                        // Update fit zoom on window resize
                        defaultView.fitZoom =
                        view.ZoomFitIntoRect(CANVAS_WIDTH, CANVAS_HEIGHT, { 0, 0, GetRenderWidth() - VIEWPORT_GUI_RIGHT_PANEL_WIDTH, GetRenderHeight() - VIEWPORT_GUI_OCCLUSION_Y });
                    }

                // Mouse panning
                if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
                    {
                        Vector2 d{ GetMouseDelta() };
                        view.pan.x += d.x;
                        view.pan.y += d.y;
                    }
                else
                    // Mouse wheel zoom
                    if (!ListState.ShowList)
                        {
                            const auto  wheelSign{ std::signbit(GetMouseWheelMove()) };
                            const float wheel{ std::copysign(1.0f, GetMouseWheelMove()) * (GetMouseWheelMove() != 0.0f) };
                            const bool  canZoom{ (wheelSign && view.zoom > view.GetMinZoom()) || (!wheelSign && view.zoom < view.GetMaxZoom()) };
                            if (wheel != 0 && canZoom)
                                {
                                    view.prevZoom = view.zoom;
                                    const Vector2 mouse{ GetMousePosition() };
                                    // Zoom
                                    view.zoom = !wheelSign ? view.zoom * (1.f + ZOOM_STEP) : view.zoom * (1.f - ZOOM_STEP);
                                    view.SafelyClampZoom();
                                    // Adjust pan so that the point under the mouse remains under the mouse after zoom
                                    Vector2 canvasPointUnderMouse;
                                    canvasPointUnderMouse.x = (mouse.x - view.pan.x) / view.prevZoom;
                                    canvasPointUnderMouse.y = (mouse.y - view.pan.y) / view.prevZoom;
                                    view.pan.x              = mouse.x - canvasPointUnderMouse.x * view.zoom;
                                    view.pan.y              = mouse.y - canvasPointUnderMouse.y * view.zoom;

                                    view.SafelyClampPan(CANVAS_WIDTH, CANVAS_HEIGHT);
                                }
                        }
            }
#pragma endregion Events

#pragma region Drawing
            BeginDrawing();
            ClearBackground(GRAY);

            // Each frame rebuild the animation names vector
            CP->RebuildAnimationNamesVectorAndRefreshPropertyPanel(ListState.activeIndex);

            const bool hasValidSelectedAnimation{ ListState.activeIndex > -1 && !CP->ImmutableTransientAnimationNames.empty() && ListState.activeIndex < CP->ImmutableTransientAnimationNames.size() };

            const Rectangle canvasRect{ view.pan.x, view.pan.y, CANVAS_WIDTH * view.zoom, CANVAS_HEIGHT * view.zoom };

            // Draw sprite texture if has one
            if (CP->SpriteTexture.has_value())
                {
                    DrawTextureEx(CP->SpriteTexture.value(), view.pan, 0, view.zoom, WHITE);
                }

            // Draw grid only if snapping is enabled
            if (app.SnapToGrid)
                {
                    Vector2 gridMouseCell = { 0 };
                    GuiGrid(canvasRect, "Canvas", (app.GridSize * view.zoom), 1,
                    &gridMouseCell); // Draw a fancy grid

                    // Draw outline
                    DrawRectangleLinesEx(canvasRect, 1.f, BLACK);
                }

            // Draw canvas origin XY axis
            {
                constexpr int32_t AXIS_LEN{ std::numeric_limits<int32_t>::max() };
                DrawLineEx((view.pan), to::Vector2_({ AXIS_LEN, (int)view.pan.y }), 2.f, RED);
                DrawLineEx((view.pan), to::Vector2_({ (int)view.pan.x, AXIS_LEN }), 2.f, GREEN);
            }

            // Draw the selected animation
            if (hasValidSelectedAnimation)
                {
                    auto& animationVariant = CP->AnimationNameToSpritesheet.at(CP->ImmutableTransientAnimationNames[ListState.activeIndex]);
                    if (std::holds_alternative<SpritesheetUv>(animationVariant.Data))
                        {
                            auto& spriteSheet = std::get<SpritesheetUv>(animationVariant.Data);

                            DrawUVRectDashed(to::Rectangle_(spriteSheet.Uv), view);

                            for (int32_t i{ 1 }; i < spriteSheet.Property_NumOfFrames.Value; ++i)
                                {
                                    Rectangle frameUv{ to::Rectangle_(spriteSheet.Uv) };
                                    frameUv.x += (i % spriteSheet.Property_Columns.Value) * frameUv.width;
                                    frameUv.y += (i / spriteSheet.Property_Columns.Value) * frameUv.height;
                                    DrawUVRectDashed(frameUv, view);
                                }

                            // DrawRectangleRec(spriteSheet.Uv, RED);
                            const auto g{ app.GridSize };

                            constexpr float baseControlExtent{ 5.f };
                            const auto      controlExtent{ baseControlExtent };
                            const int32_t   focusedControlPoints{ DrawUvRectControlsGetControlIndex(to::Rectangle_(spriteSheet.Uv), view, controlExtent) };
                            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                                {
                                    spriteSheet.DraggingControlIndex = focusedControlPoints;
                                }
                            else if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && spriteSheet.DraggingControlIndex != EControlIndex::NONE)
                                {
                                    spriteSheet.DraggingControlIndex = EControlIndex::NONE;

                                    // Normalize sane rectangle always positive values
                                    if (spriteSheet.Uv.w < 0)
                                        {
                                            spriteSheet.Uv.w *= -1;
                                            spriteSheet.Uv.x -= spriteSheet.Uv.w;
                                        }
                                    spriteSheet.Uv.w = std::max(app.SnapToGrid ? g : 1, spriteSheet.Uv.w);
                                    if (spriteSheet.Uv.h < 0)
                                        {
                                            spriteSheet.Uv.h *= -1.f;
                                            spriteSheet.Uv.y -= spriteSheet.Uv.h;
                                        }
                                    spriteSheet.Uv.h = std::max(app.SnapToGrid ? g : 1, spriteSheet.Uv.h);
                                }

                            // Get mouse pos in image space
                            Vec2 mousePos{};
                            {
                                auto rayMousePos{ GetMousePosition() };
                                rayMousePos.x = (rayMousePos.x * (1.f / view.zoom)) - view.pan.x;
                                rayMousePos.y = (rayMousePos.y * (1.f / view.zoom)) - view.pan.y;
                                mousePos      = from::Vector2_(rayMousePos);
                            }

                            // DrawRectanglePro({ 0,0,100 * zoom,100 * zoom }, mousePos, 0.f, RED);

                            RoundTo(mousePos.x, g, app.SnapToGrid);
                            RoundTo(mousePos.y, g, app.SnapToGrid);
                            // printf("Mousepos %f %f\n", mousePos.x, mousePos.y);
                            // DrawRectangleRec({ mousePos.x, mousePos.y, 10 * zoom, 10 * zoom }, YELLOW);
                            // DrawUVRectDashed({ mousePos.x, mousePos.y, 10, 10 });

                            // If zoom has changed
                            if (view.prevZoom != view.zoom)
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
                                            RoundTo(tempY, g, app.SnapToGrid);
                                            const auto movDiff{ tempY - spriteSheet.Uv.y };
                                            spriteSheet.Uv.y = tempY;
                                            spriteSheet.Uv.h -= std::copysignf(movDiff, mouseMov.y * -1.f);
                                        }
                                    if (spriteSheet.DraggingControlIndex & EControlIndex::BOTTOM)
                                        {
                                            spriteSheet.Uv.h -= mouseMov.y;
                                            RoundTo(spriteSheet.Uv.h, g, app.SnapToGrid);
                                        }
                                    if (spriteSheet.DraggingControlIndex & EControlIndex::LEFT && mouseMov.x != 0.f)
                                        {
                                            spriteSheet.Uv.x -= mouseMov.x;
                                            spriteSheet.Uv.w += mouseMov.x;
                                            std::cout << "MouseMovX:" << mouseMov.x << " width:" << spriteSheet.Uv.w << std::endl;
                                            RoundTo(spriteSheet.Uv.x, g, app.SnapToGrid);
                                        }
                                    if (spriteSheet.DraggingControlIndex & EControlIndex::RIGHT)
                                        {
                                            spriteSheet.Uv.w -= mouseMov.x;
                                            RoundTo(spriteSheet.Uv.w, g, app.SnapToGrid);
                                        }
                                }
                            // Update mouse delta at the end
                            spriteSheet.DeltaMousePos = mousePos;
                        }
                }
#pragma region GUI

            const char* animationNameOrPlaceholder{ !hasValidSelectedAnimation ? "No animation" : CP->ImmutableTransientAnimationNames[ListState.activeIndex] };

            DrawRectangle(0, 0, GetRenderWidth(), 50, DARKGRAY);
            float TITLE_X_OFFSET{ PAD };

            if (ActiveModal != EModalType::NONE)
                {
                    GuiLock();
                }

            // Open sprite button
            const Rectangle openButtonRect{ TITLE_X_OFFSET, PAD, GetStringWidth("Open sprite") * 1.f + PAD, 30 };
            if (GuiButton(openButtonRect, "Open sprite"))
                {
                    std::string newImagePath{};

                    if (CP->HasUnsavedChanges())
                        {
                            // ActiveModal = EModalType::CONFIRM_DISCARD_CHANGES;
                        }

                    // Since raylib relies on stb_image for image loading right now the formats supported are:
                    if (app.OpenFileDialog(newImagePath, { "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tga", "*.gif" }))
                        {
                            std::cout << "Trying to load:" << newImagePath << std::endl;
                            // Load texture if possible
                            auto newProject{ std::make_unique<Project>() };
                            if (newProject->LoadFromFile(newImagePath))
                                {
                                    CP = std::move(newProject);

                                    // Reset view
                                    {
                                        // Set the zoom to fit the new image on the max size
                                        defaultView.fitZoom = view.ZoomFitIntoRect(
                                        CP->SpriteTexture->width, CP->SpriteTexture->height, { 0, 0, GetRenderWidth() - VIEWPORT_GUI_RIGHT_PANEL_WIDTH, GetRenderHeight() - VIEWPORT_GUI_OCCLUSION_Y });
                                        defaultView.zoom = defaultView.fitZoom;

                                        ResetViewToDefault();

                                        // Reset selection
                                        ListState = ListSelection{};
                                    }
                                }
                            else
                                {
                                    app.LastError = "Failed to load image!";
                                }
                        }
                }
            TITLE_X_OFFSET += openButtonRect.width + PAD;

            // Draw grid size
            {
                const Rectangle rect{ TITLE_X_OFFSET, PAD, GetStringWidth("Grid size") + 80.f, 30 };
                (void)(NumericBox(rect, "Grid size", &app.GridSize, 0, 8196, app.GridSizeInputActive));

                TITLE_X_OFFSET += rect.width + PAD;

                // Snap to grid
                {
                    GuiDrawRectangle({ TITLE_X_OFFSET, PAD, 80, 30 }, 1, GRAY, LIGHTGRAY);
                    TITLE_X_OFFSET += PAD / 2;
                    GuiCheckBox({ TITLE_X_OFFSET, PAD + 5, 20, 20 }, "Snap", &app.SnapToGrid);
                }
                TITLE_X_OFFSET += 80.f;
            }

            {
                // Reset zoom to fit
                const Rectangle fitViewRect{ TITLE_X_OFFSET, PAD, GetStringWidth("Fit view"), 30 };
                if (GuiButton(fitViewRect, "Fit view"))
                    {
                        ResetViewToDefault();
                    }
                TITLE_X_OFFSET += fitViewRect.width + PAD;
            }

            {
                // Create new animation

                const Rectangle newAnimRect{ TITLE_X_OFFSET, PAD, GetStringWidth("Add"), 30 };
                if (GuiButton(newAnimRect, "Add"))
                    {
                        ActiveModal = EModalType::CREATE_ANIMATION;
                    }
                TITLE_X_OFFSET += newAnimRect.width + PAD;

                // Delete animation
                if (hasValidSelectedAnimation)
                    {
                        const Rectangle delAnimRect{ TITLE_X_OFFSET, PAD, GetStringWidth("Delete"), 30 };
                        if (GuiButton(delAnimRect, "Delete"))
                            {
                                ActiveModal = EModalType::CONFIRM_DELETE;
                            }
                        TITLE_X_OFFSET += delAnimRect.width + PAD;
                    }
            }

            // Animation selection
            {
                const auto nameW{ std::max(150.f, GetStringWidth(animationNameOrPlaceholder)) };

                const Rectangle animNameRect{ TITLE_X_OFFSET, PAD, nameW, 30 };
                if (GuiButton(animNameRect, animationNameOrPlaceholder))
                    {
                        ListState.ShowList = !ListState.ShowList;
                    }
                const auto scrollHeight{ std::clamp(CP->ImmutableTransientAnimationNames.size() * 50.f, 100.f, 500.f) };
                const auto maxStringW{ std::accumulate(
                CP->ImmutableTransientAnimationNames.begin(), CP->ImmutableTransientAnimationNames.end(), nameW, [](float acc, const char* name) { return std::max(acc, GetStringWidth(name)); }) };

                if (ListState.ShowList)
                    {
                        const ListSelection prevState{ ListState };

                        const Rectangle panelScrollRect{ TITLE_X_OFFSET - PAD, PAD + 30, maxStringW + PAD * 2, scrollHeight };
                        const Rectangle animListRect{ TITLE_X_OFFSET - PAD, PAD + 30, maxStringW + PAD * 2, scrollHeight };
                        GuiScrollPanel(panelScrollRect, NULL, animListRect, &panelScroll, &panelView);
                        BeginScissorMode(panelView.x, panelView.y, panelView.width, panelView.height);
                        GuiListViewEx(
                        animListRect, CP->ImmutableTransientAnimationNames.data(), CP->ImmutableTransientAnimationNames.size(), &ListState.scrollIndex, &ListState.activeIndex, &ListState.focusIndex);
                        // Clamp into range
                        ListState.activeIndex = std::clamp(ListState.activeIndex, -1, static_cast<int32_t>(CP->ImmutableTransientAnimationNames.size()) - 1);
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
                        // if (GuiDropdownBox({ TITLE_X_OFFSET, PAD, 150, 30 }, "Spritesheet;Keyframe", &PropertyPanel->GuiAnimTypeIndex, PropertyPanel->ShowAnimTypeDropDown))
                        //{
                        //	PropertyPanel->ShowAnimTypeDropDown = !PropertyPanel->ShowAnimTypeDropDown;

                        //	switch (PropertyPanel.GuiAnimTypeIndex)
                        //	{
                        //	case 0:PropertyPanel.AnimationType = EAnimationType::SPRITESHEET; break;
                        //	case 1:PropertyPanel.AnimationType = EAnimationType::KEYFRAME; break;
                        //	default: break;
                        //	}
                        //}
                        // TITLE_X_OFFSET += 150 + PAD;
                    }
            }

            // Property panel
            {
                constexpr float RIGHTPANEL_W{ 380.f };
                const float     RIGHTPANEL_X{ GetRenderWidth() - RIGHTPANEL_W };
                float           RIGHTPANEL_Y{ 50.f };
                GuiDrawRectangle({ RIGHTPANEL_X, RIGHTPANEL_Y, RIGHTPANEL_W, GetRenderHeight() - RIGHTPANEL_Y }, 1, GRAY, DARKGRAY);

                GuiDrawText(animationNameOrPlaceholder, { RIGHTPANEL_X, RIGHTPANEL_Y, RIGHTPANEL_W, 30 }, TEXT_ALIGN_CENTER, LIGHTGRAY);
                RIGHTPANEL_Y += 30 + PAD;
                // Draw properties only if selected
                if (hasValidSelectedAnimation)
                    {
                        DrawPropertiesIfValidPtr(Rectangle{ RIGHTPANEL_X + PAD, RIGHTPANEL_Y, RIGHTPANEL_W - PAD * 2.f, GetRenderHeight() - RIGHTPANEL_Y }, CP->PropertyPanel);
                    }
            }

            // Draw error string messagebox
            if (app.LastError.has_value())
                {
                    // DrawText(LastError->c_str(), 220, 20, 16, RED);
                    const auto result = GuiMessageBox(Rectangle{ 0, 0, (float)GetRenderWidth(), (float)GetRenderHeight() }, "Error", app.LastError->c_str(), "OK");
                    if (result > 0)
                        {
                            app.LastError.reset();
                        }
                }

            // Draw filename bottom left window
            {
                DrawRectangle(0, GetRenderHeight() - 16, GetRenderWidth(), 16, DARKGRAY);
                DrawText(app.ImagePath.c_str(), 10, GetRenderHeight() - 16, 16, WHITE);
            }

            // Unlock gui
            GuiUnlock();

            // Modals
            {
                Rectangle msgRect{ 0, 0, 600, 300 };
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
                        if (const auto found{ CP->AnimationNameToSpritesheet.find(NewAnimationName) }; found != CP->AnimationNameToSpritesheet.end())
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
                        else if (GuiButton({ msgRect.x + PAD, msgRect.y + msgRect.height - 30 - PAD, 100, 30 }, "Create"))
                            {
                                ActiveModal = EModalType::NONE;
                                // Create the animation
                                SpritesheetUv spriteSheet{};
                                spriteSheet.Uv = { 0, 0, app.GridSize, app.GridSize };

                                AnimationData animData{ std::move(spriteSheet) };
                                CP->AnimationNameToSpritesheet.emplace(std::string(NewAnimationName), std::move(animData));
                                ListState.activeIndex = CP->AnimationNameToSpritesheet.size() - 1;
                            }

                        if (GuiButton({ msgRect.x + PAD + 100 + PAD, msgRect.y + msgRect.height - 30 - PAD, 100, 30 }, "Cancel"))
                            {
                                ActiveModal = EModalType::NONE;
                            }
                    }
                else if (ActiveModal == EModalType::CONFIRM_DELETE)
                    {
                        assert(!CP->ImmutableTransientAnimationNames.empty());
                        std::string tmp{ "Delete " };
                        tmp += CP->ImmutableTransientAnimationNames[ListState.activeIndex];

                        if (GuiMessageBox(msgRect, "Confirm delete", tmp.c_str(), "Cancel;Delete") == 2)
                            {
                                ActiveModal = EModalType::NONE;

                                CP->AnimationNameToSpritesheet.erase(CP->ImmutableTransientAnimationNames[ListState.activeIndex]);
                                ListState.activeIndex = -1;
                                CP->RebuildAnimationNamesVectorAndRefreshPropertyPanel(ListState.activeIndex);
                            }
                    }
            }

#pragma endregion GUI

            EndDrawing();

#pragma endregion Drawing
        }

    return 0;
}
