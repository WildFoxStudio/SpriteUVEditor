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

#include "app.hpp"

#include "tinyfiledialogs.h"

#include <iostream>

App::App(int32_t width, int32_t height, const char* title)
{
    SetConfigFlags(FLAG_WINDOW_MAXIMIZED | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1600, 900, title);

    SetTargetFPS(GetMonitorRefreshRate(0));

    Image icon = LoadImage("icons/uvEdit.png");
    if (icon.data)
        {
            SetWindowIcon(icon);
            UnloadImage(icon);
        }
    else
        {
            std::cout << "Failed to load window icon!" << std::endl;
        }

    fontRoboto = LoadFontEx("fonts/Roboto-Bold.ttf", 16, nullptr, 250);
    if (!fontRoboto.texture.id)
        {
            std::cout << "Failed to load Roboto font, falling back to default font." << std::endl;
        }

    // Create the checkerboard texture
    const int32_t CHECKER_SIZE{ 16 };
    Image         checkerImage = GenImageChecked(CHECKER_SIZE * 2, CHECKER_SIZE * 2, CHECKER_SIZE, CHECKER_SIZE, Color{ 130, 130, 130, 255 }, Color{ 160, 160, 160, 255 });
    CheckerBoardTexture        = LoadTextureFromImage(checkerImage);
    if (CheckerBoardTexture.id == 0)
        {
            std::cout << "Failed to create checkerboard texture!" << std::endl;
        }
}

App::~App()
{
    if (fontRoboto.texture.id)
        {
            UnloadFont(fontRoboto);
        }
    if (CheckerBoardTexture.id)
        {
            UnloadTexture(CheckerBoardTexture);
        }
    CloseWindow();
}

bool
App::ShouldRun() const
{
    return !WindowShouldClose();
}

bool
App::OpenFileDialog(std::string& filePath, const std::vector<std::string>& extension) const
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
