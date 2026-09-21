#include "LaunchOptions.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
LaunchOptions parse(std::initializer_list<std::string_view> args){return parseLaunchOptions(std::span(args.begin(),args.size()));}
int main() try {
 auto a=parse({});check(a.region=="practice_room"&&!a.debugHud&&!a.vsync&&!a.help,"defaults");
 check(parse({"--help"}).help,"help");
 a=parse({"--debug-hud","--vsync"});check(a.region=="practice_room"&&a.debugHud&&a.vsync,"options only");
 for(const auto& args:std::vector<std::vector<std::string_view>>{{"avatar_lake","--debug-hud","--vsync"},{"--debug-hud","avatar_lake","--vsync"},{"--vsync","--debug-hud","avatar_lake"}}){
  a=parseLaunchOptions(args);check(a.region=="avatar_lake"&&a.debugHud&&a.vsync,"region anywhere");
 }
 a=parse({"--vsync","--vsync","--help","--help","--debug-hud","--debug-hud"});check(a.vsync&&a.help&&a.debugHud,"duplicate flags idempotent");
 a=parse({"avatar_lake","--tileset","X"});check(a.region=="avatar_lake"&&a.tileset=="X","existing launcher shape");
 check(parse({"--decorations","none"}).decorations=="none","decoration special value preserved");
 for(const auto& args:std::vector<std::vector<std::string_view>>{{"--decorations"},{"--decorations","--vsync"},{"--tileset"},{"--tileset",""},{"--unknown"},{"a","b"},{"--tileset","a","--tileset","b"},{"--decorations","a","--decorations","b"}}){
  bool rejected=false;try{parseLaunchOptions(args);}catch(const std::exception& e){rejected=true;if(args.size()==1&&args[0]=="--decorations")check(std::string(e.what()).find("--decorations")!=std::string::npos,"missing value identifies option");}
  check(rejected,"bad arguments rejected");
 }
 std::cout<<"PASS launch defaults, flags, ordering, launcher compatibility and errors\n";
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
