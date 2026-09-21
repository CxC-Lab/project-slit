#pragma once
#include <span>
#include <string>
#include <string_view>
struct LaunchOptions {
    std::string region="practice_room",tileset,decorations;
    bool debugHud=false,vsync=false,help=false;
};
LaunchOptions parseLaunchOptions(std::span<const std::string_view> arguments);
const char* launchHelp();
