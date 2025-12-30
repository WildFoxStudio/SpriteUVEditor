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

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "raylib.h"

class App final
{
  public:
    int32_t                    GridSize{ 64 };
    bool                       GridSizeInputActive{}; // Gui box active state
    bool                       DrawGrid{ true };
    bool                       SnapToGrid{ true };
    std::optional<std::string> LastError{};
    Texture2D                  CheckerBoardTexture{};

    App(int32_t width, int32_t height, const char* title);
    ~App();
    bool ShouldRun() const;

    Font GetFont() const { return fontRoboto; }

    bool OpenFileDialog(std::string& filePath, const std::vector<std::string>& extension) const;

  private:
    Font fontRoboto{};
};
