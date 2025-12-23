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
#include "project.hpp"

#include <nlohmann/json.hpp>

#include <cassert>
#include <fstream>
#include <optional>

namespace
{
std::optional<std::string>
LoadSpriteTexture(const std::string& imagePath, std::optional<Texture2D>& outTexture)
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
};

bool
Project::SaveToFile() const
{
    if (SpritePath.empty())
        return false;

    assert(SpriteTexture.has_value()); // Should have a texture loaded
    assert(false); // Not implemented yet
    return false;
}

bool
Project::LoadFromFile(const std::string& filePath)
{
    assert(!filePath.empty());
    assert(!SpriteTexture.has_value()); // Should not have a texture already loaded

    std::optional<Texture2D> loadedTexture{};
    const auto               loadError{ LoadSpriteTexture(filePath, loadedTexture) };
    if (!loadError.has_value())
        {
            SpriteTexture = std::move(loadedTexture.value());
            SpritePath    = filePath;
            return true;
        }

    // Try to load the equivalent json with the same name
    nlohmann::json j{};
    try
        {
            std::ifstream fileStream{ filePath };
            fileStream >> j;
        }
    catch (const std::exception& e)
        {
            // Failed to open the file, who cares it might not exist yet.
        }

    return SpriteTexture.has_value();
}

bool
Project::HasUnsavedChanges()
{
    // Compare latest json vs previous json
    return true;
}

Project::Project(Texture2D sprite, const std::string& filePath) : SpritePath{ filePath }, SpriteTexture{ sprite }
{
    assert(!filePath.empty());
    assert(sprite.id > 0);
}
