#include "LaunchOptions.hpp"
#include <stdexcept>
LaunchOptions parseLaunchOptions(std::span<const std::string_view> args) {
    LaunchOptions result;bool regionSeen=false;
    for(std::size_t i=0;i<args.size();++i){const auto arg=args[i];
        if(arg=="--debug-hud")result.debugHud=true;
        else if(arg=="--vsync")result.vsync=true;
        else if(arg=="--help")result.help=true;
        else if(arg=="--tileset"||arg=="--decorations"){
            auto& field=arg=="--tileset"?result.tileset:result.decorations;
            if(!field.empty())throw std::runtime_error("Duplicate option: "+std::string(arg));
            if(i+1==args.size()||args[i+1].empty()||args[i+1].starts_with("-"))throw std::runtime_error("Missing value for "+std::string(arg));
            field=args[++i];
        } else if(arg.empty()||arg.starts_with("-"))throw std::runtime_error("Unknown option: "+std::string(arg));
        else {if(regionSeen)throw std::runtime_error("Only one region argument is allowed");result.region=arg;regionSeen=true;}
    }return result;
}
const char* launchHelp(){return R"(Usage:
  project_slit.exe [region] [options]

Arguments:
  region                     Region name (default: practice_room)

Options:
  --tileset <name>           Override terrain tileset; disables default decorations
  --decorations <json>       Override decorations JSON (none: disabled); beats tileset override
  --debug-hud                Start with debug HUD enabled
  --vsync                    Enable VSync (default OFF; ON removes manual 60 FPS limit)
  --help                     Print options and exit without creating a window

Controls:
  F3                         Toggle debug HUD
  F12                        Save screen PNG and coordinate JSON to build/preview/captured_XXXX.*
  Alt+Enter                  Toggle borderless fullscreen / windowed
)";}
