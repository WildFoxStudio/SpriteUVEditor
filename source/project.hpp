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

#include "geometry.hpp"

#include <cstdint>
#include <string_view>
#include <string>
#include <variant>
#include <vector>
#include <map>

struct Property {
	int32_t Value{};
	bool ActiveBox{};
};

struct NamedProperty {
	const std::string_view Name;
	Property *const Prop;
};

struct SpritesheetUv {
	Rect Uv{};

	//Properties
	Property Property_Rect[4]{};
	Property Property_AnimTypeIndex{};
	Property Property_NumOfFrames{ 1 };
	Property Property_Columns{ std::numeric_limits<int32_t>::max() };
	Property Property_FrameDurationMs{ 100 };
	bool Looping{ true };

#pragma region Internal data
	Property CurrentFrameIndex{};
	int64_t StartTimeMs{};

	int32_t DraggingControlIndex{};
	Vec2 DeltaMousePos{};
#pragma endregion
};

struct KeyframeUv {
	struct Keyframe {
		Rectangle Uv{};
		int32_t FrameDurationMs{ 100 };
	};
	std::vector<Keyframe> Keyframes{};
};

using AnimationVariant_T = std::variant<SpritesheetUv, KeyframeUv>;

struct AnimationData {
	AnimationVariant_T Data{ SpritesheetUv{} };
};

class Project {
    public:
	bool SaveToFile(const std::string &filePath) const;
	bool LoadFromFile(const std::string &filePath);

	std::vector<const char *> ImmutableTransientAnimationNames{};
	void RebuildAnimationNamesVector()
	{
		ImmutableTransientAnimationNames.clear();
		ImmutableTransientAnimationNames.reserve(
			AnimationNameToSpritesheet.size());
		for (const auto &[name, spriteSheet] :
		     AnimationNameToSpritesheet) {
			ImmutableTransientAnimationNames.push_back(name.data());
		}
	}

    private:
	Project(Texture2D sprite);
	std::string spritePath{};
	Texture2D texture{};
	std::map<std::string, AnimationData> AnimationNameToSpritesheet{};
};
