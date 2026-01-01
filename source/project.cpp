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

#include <cassert>
#include <filesystem>
#include <fstream>
#include <optional>

/** Example json structure
 *
{
  "animations" : [
    {
      "name":"Idle",
      "type":"Spritesheet",
      "x": "0",
      "y": "0",
      "width": "32",
      "height": "32",
      "frames":"8",
      "columns":"4",
      "durationMs":"350"
      "loop":"true"
    }
    ]
}
 */

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

    // Write the latest json to file
    nlohmann::ordered_json j{ SerializeAnimationData() };
    try
        {
            // remove extension from path and add .json
            const auto    spriteJsonPath{ std::filesystem::path{ SpritePath }.replace_extension(".json").string() };
            std::ofstream fileStream{ spriteJsonPath };
            fileStream << j.dump(4);
            fileStream.close();
            return true;
        }
    catch (const std::exception& e)
        {
            // Failed to open the file
            return false;
        }

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
        }

    // Try to load the equivalent json with the same name
    nlohmann::ordered_json j{};
    try
        {
            const auto    spriteJsonPath{ std::filesystem::path{ SpritePath }.replace_extension(".json").string() };
            std::ifstream fileStream{ spriteJsonPath };
            fileStream >> j;
            Deserialize(j);
        }
    catch (const std::exception& e)
        {
            // Failed to open the file, who cares it might not exist yet.
        }

    _actionsStack.clear();
    _redoStack.clear();

    CommitNewAction();

    return SpriteTexture.has_value();
}

bool
Project::ExportHeaderFile(const std::string& headerFilePath) const
{
    // Export all animations names as `const char Animation0[] = {"Animation0"};`
    try
        {
            std::ofstream fileStream{ headerFilePath };
            fileStream << "#pragma once\n\n";
            for (const auto& [name, animationData] : AnimationNameToSpritesheet)
                {
                    fileStream << "const char " << name << "[] = {\"" << name << "\"};\n";
                }
            fileStream.close();
            return true;
        }
    catch (const std::exception& e)
        {
            // Failed to open the file
            return false;
        }
    return false;
}

bool
Project::HasUnsavedChanges()
{
    // Compare latest json vs previous json
    if (_actionsStack.empty())
        {
            return false;
        }

    // Read latest serialized json
    try
        {
            std::ifstream fileStream{};
            const auto    spriteJsonPath{ std::filesystem::path{ SpritePath }.replace_extension(".json").string() };
            {
                fileStream.open(spriteJsonPath);
                nlohmann::ordered_json latestJson{};
                fileStream >> latestJson;
                fileStream.close();
                // Compare latest json vs previous json
                return latestJson != SerializeAnimationData();
            }
        }
    catch (const std::exception& e)
        {
            // Failed to open the file, assume there are changes because file might not exist yet.
            return !AnimationNameToSpritesheet.empty();
        }

    return false;
}

void
Project::CommitNewAction()
{
    auto newState{ SerializeAnimationData() };
    // Prevent from pushing the same state twice in the stack
    if (!_actionsStack.empty() && _actionsStack.back() == newState)
        {
            return;
        }
    _actionsStack.push_back(std::move(newState));
    // Clear redo stack
    _redoStack.clear();

    // Enforce max undo actions by dropping the oldest states
    while (_actionsStack.size() > MAX_UNDO_ACTIONS)
        {
            _actionsStack.pop_front();
        }
}

void
Project::UndoAction()
{
    if (_actionsStack.empty())
        {
            return;
        }

    // Remove current state and store it in redo stack
    auto current{ std::move(_actionsStack.back()) };
    _actionsStack.pop_back();

    if (!_actionsStack.empty())
        {
            // Restore the previous state (now the last in the actions stack)
            const auto previous = _actionsStack.back();
            Deserialize(previous);
        }
    else
        {
            // No previous state exists -> restore an empty/default state
            nlohmann::ordered_json emptyState{};
            emptyState["animations"]             = nlohmann::ordered_json::array();
            emptyState["selectedAnimationIndex"] = -1;
            Deserialize(emptyState);
        }

    // Add the removed current state to redo stack
    _redoStack.push_back(std::move(current));
}

void
Project::RedoAction()
{
    if (_redoStack.empty())
        {
            return;
        }
    auto j{ std::move(_redoStack.back()) };
    _redoStack.pop_back();

    Deserialize(j);

    // Add to actions stack and enforce max size
    _actionsStack.push_back(std::move(j));
    while (_actionsStack.size() > MAX_UNDO_ACTIONS)
        {
            _actionsStack.pop_front();
        }
}

void
Project::Deserialize(const nlohmann::ordered_json& j)
{
    // Clear everything
    AnimationNameToSpritesheet.clear();
    // Deserialize animations
    for (const auto& animJson : j.at("animations"))
        {
            const std::string name{ animJson.at("name").get<std::string>() };
            const std::string type{ animJson.at("type").get<std::string>() };
            if (type == "Spritesheet")
                {
                    SpritesheetUv spriteSheet{};
                    spriteSheet.Uv.x                           = animJson.at("x").get<int32_t>();
                    spriteSheet.Uv.y                           = animJson.at("y").get<int32_t>();
                    spriteSheet.Uv.w                           = animJson.at("width").get<int32_t>();
                    spriteSheet.Uv.h                           = animJson.at("height").get<int32_t>();
                    spriteSheet.Property_NumOfFrames.Value     = animJson.at("frames").get<int32_t>();
                    spriteSheet.Property_Columns.Value         = animJson.at("columns").get<int32_t>();
                    spriteSheet.Property_FrameDurationMs.Value = animJson.at("durationMs").get<int32_t>();
                    spriteSheet.Looping                        = animJson.at("looping").get<bool>();

                    AnimationData animData{ std::move(spriteSheet) };

                    AnimationNameToSpritesheet.emplace(name, std::move(animData));
                }
            else if (type == "Keyframe")
                {
                    assert(false && "KEYFRAME not Supported yet!");
                }
        }
    // Editor only data
    const int32_t selectedAnimationIndex{ j.at("selectedAnimationIndex").get<int32_t>() };
    assert(selectedAnimationIndex >= -1 && selectedAnimationIndex < static_cast<int32_t>(AnimationNameToSpritesheet.size()));
    ListState.activeIndex = selectedAnimationIndex;
    ListState.scrollIndex = selectedAnimationIndex;
    ListState.focusIndex  = selectedAnimationIndex;
    RebuildAnimationNamesVectorAndRefreshPropertyPanel(ListState.activeIndex);
}

nlohmann::ordered_json
Project::SerializeAnimationData() const
{
    nlohmann::ordered_json j{};
    // Serialize animations
    j["animations"] = nlohmann::ordered_json::array();
    for (const auto& [name, animationData] : AnimationNameToSpritesheet)
        {
            nlohmann::ordered_json animJson{};
            animJson["name"] = name;
            if (std::holds_alternative<SpritesheetUv>(animationData.Data))
                {
                    const auto& spriteSheet{ std::get<SpritesheetUv>(animationData.Data) };
                    animJson["type"]       = "Spritesheet";
                    animJson["x"]          = spriteSheet.Uv.x;
                    animJson["y"]          = spriteSheet.Uv.y;
                    animJson["width"]      = spriteSheet.Uv.w;
                    animJson["height"]     = spriteSheet.Uv.h;
                    animJson["frames"]     = spriteSheet.Property_NumOfFrames.Value;
                    animJson["columns"]    = spriteSheet.Property_Columns.Value;
                    animJson["durationMs"] = spriteSheet.Property_FrameDurationMs.Value;
                    animJson["looping"]    = spriteSheet.Looping;
                }
            else if (std::holds_alternative<KeyframeUv>(animationData.Data))
                {
                    assert(false && "KEYFRAME not Supported yet!");
                }
            j["animations"].push_back(animJson);
        }

    // Editor only data
    j["selectedAnimationIndex"] = ListState.activeIndex;

    return j;
}

Project::Project(Texture2D sprite, const std::string& filePath) : SpritePath{ filePath }, SpriteTexture{ sprite }
{
    assert(!filePath.empty());
    assert(sprite.id > 0);
}
