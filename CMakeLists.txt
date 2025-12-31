cmake_minimum_required(VERSION 3.15)
project(SpriteUVEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

# -------------------------------------------------
# 1. raylib (proper Git repository)
# -------------------------------------------------
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# -------------------------------------------------
# 2. raygui – single header from GitHub
# -------------------------------------------------
FetchContent_Declare(
    raygui
    GIT_REPOSITORY https://github.com/raysan5/raygui.git
    GIT_TAG 4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raygui)

# Create an interface library for raygui
add_library(raygui INTERFACE)
target_include_directories(raygui INTERFACE "${raygui_SOURCE_DIR}/src")

# -------------------------------------------------
# 3. nlohmann::json – using FetchContent properly
# -------------------------------------------------
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

# -------------------------------------------------
# 4. Your executable
# -------------------------------------------------
# -------------------------------------------------
# 4. tinyfiledialogs - using FetchContent
# -------------------------------------------------
FetchContent_Declare(
    tinyfiledialogs
    GIT_REPOSITORY https://github.com/native-toolkit/tinyfiledialogs.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tinyfiledialogs)

# Create a library for tinyfiledialogs if it doesn't define one
# (It usually just provides source, so we define it manually to be safe and clean)
if(NOT TARGET tinyfiledialogs)
    add_library(tinyfiledialogs 
        "${tinyfiledialogs_SOURCE_DIR}/tinyfiledialogs.c"
        "${tinyfiledialogs_SOURCE_DIR}/tinyfiledialogs.h"
    )
    target_include_directories(tinyfiledialogs PUBLIC "${tinyfiledialogs_SOURCE_DIR}")
    # tinyfiledialogs needs specific linking on Windows
    if(WIN32)
        target_link_libraries(tinyfiledialogs PUBLIC comdlg32 ole32)
    endif()
endif()

set_target_properties(tinyfiledialogs PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )

# Ensure tinyfiledialogs uses the same runtime
if(WIN32)
target_compile_options(tinyfiledialogs PRIVATE 
            $<$<CONFIG:Release>:/MT> 
            $<$<CONFIG:Debug>:/MTd>
        )
endif()

# -------------------------------------------------
# 5. Your executable
# -------------------------------------------------
add_executable(sprite_uv_editor main.cpp source/definitions.hpp source/app.hpp source/geometry.hpp source/project.hpp source/drawing.hpp)

target_sources(sprite_uv_editor PRIVATE 
    source/app.cpp 
    source/project.cpp
    sprite_uv_editor.rc
)

target_link_libraries(sprite_uv_editor PRIVATE 
    raylib 
    raygui 
    nlohmann_json::nlohmann_json
    tinyfiledialogs
)

# target_include_directories is handled by linking to the library targets
# but we can keep source if you have other local headers there
target_include_directories(sprite_uv_editor PRIVATE "source")

# Optional: define RAYGUI_IMPLEMENTATION in one CPP file
target_compile_definitions(sprite_uv_editor PRIVATE RAYGUI_IMPLEMENTATION)

# Use the same runtime
# Set the debug working directory to the content folder
set_target_properties(sprite_uv_editor PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/content"
)


# Copy resources to build directory if needed
# add_custom_command(TARGET sprite_uv_editor POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#     "${CMAKE_SOURCE_DIR}/spritesheet.png"
#     $<TARGET_FILE_DIR:sprite_uv_editor>)