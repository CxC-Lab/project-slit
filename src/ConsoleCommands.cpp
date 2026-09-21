#include "ConsoleCommands.hpp"
#include <array>
#include <algorithm>
#include <cctype>
#include <sstream>
namespace {
using Args=std::vector<std::string>;
using Handler=ConsoleResult(*)(const Args&,const RuntimeControls&);
struct Command { const char* name;const char* usage; Handler handler; };
ConsoleResult help(const Args&,const RuntimeControls&);
ConsoleResult clear(const Args& args,const RuntimeControls&){return args.size()==1?ConsoleResult{{},true}:ConsoleResult{{"Usage: clear"}};}
ConsoleResult toggle(const Args& args,const std::function<void(bool)>& setter,const char* name,const char* label){
    if(args.size()!=2||(args[1]!="0"&&args[1]!="1"))return {{std::string("Usage: ")+name+" <0|1>"}};
    const bool value=args[1]=="1";setter(value);return {{std::string(label)+(value?" enabled":" disabled")}};
}
ConsoleResult debug(const Args& a,const RuntimeControls& c){return toggle(a,c.setDebugHud,"drawdebug","Debug HUD");}
ConsoleResult sync(const Args& a,const RuntimeControls& c){return toggle(a,c.setVSync,"vsync","VSync");}
const std::array<Command,4> commands{{{"drawdebug","drawdebug <0|1>",debug},{"vsync","vsync <0|1>",sync},{"help","help",help},{"clear","clear",clear}}};
ConsoleResult help(const Args& a,const RuntimeControls&){
    if(a.size()!=1)return {{"Usage: help"}};
    ConsoleResult result;for(const auto& command:commands)result.lines.emplace_back(command.usage);return result;
}
}
ConsoleResult executeConsoleCommand(std::string_view line,const RuntimeControls& controls){
    std::istringstream input{std::string(line)};Args args;for(std::string token;input>>token;)args.push_back(token);
    if(args.empty())return {};
    std::transform(args[0].begin(),args[0].end(),args[0].begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    const auto command=std::find_if(commands.begin(),commands.end(),[&](const auto& c){return args[0]==c.name;});
    if(command==commands.end())return {{"Unknown command: "+args[0]}};
    return command->handler(args,controls);
}
