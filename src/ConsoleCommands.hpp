#pragma once
#include <functional>
#include <string>
#include <string_view>
#include <vector>
struct RuntimeControls { std::function<void(bool)> setDebugHud; std::function<void(bool)> setVSync; };
struct ConsoleResult { std::vector<std::string> lines; bool clearHistory=false; };
ConsoleResult executeConsoleCommand(std::string_view line,const RuntimeControls& controls);
