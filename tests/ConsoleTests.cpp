#include "ConsoleCommands.hpp"
#include "DevConsole.hpp"
#include "Display.hpp"
#include "DebugHud.hpp"
#include "PixelFont.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
sf::Event key(sf::Keyboard::Key code,bool alt=false){return sf::Event::KeyPressed{.code=code,.alt=alt};}
int main() try {
    bool hud=false,vsync=false;int hudCalls=0,syncCalls=0;
    RuntimeControls fake{[&](bool value){hud=value;++hudCalls;},[&](bool value){vsync=value;++syncCalls;}};
    auto run=[&](std::string_view s){return executeConsoleCommand(s,fake);};
    check(run(" \t").lines.empty(),"empty command");
    check(run("  DRAWDEBUG   1 ").lines==std::vector<std::string>{"Debug HUD enabled"}&&hud&&hudCalls==1,"case/space/set HUD");
    check(run("drawdebug 0").lines[0]=="Debug HUD disabled"&&!hud&&hudCalls==2,"HUD off");
    check(run("VSYNC 1").lines[0]=="VSync enabled"&&vsync&&syncCalls==1,"vsync on");
    check(run("vsync 0").lines[0]=="VSync disabled"&&!vsync&&syncCalls==2,"vsync off");
    for(const auto* name:{"vsync","drawdebug"})for(const auto* suffix:{""," 2"," 1 extra"," yes"})
        check(run(std::string(name)+suffix).lines[0]==std::string("Usage: ")+name+" <0|1>","invalid arguments usage");
    check(hudCalls==2&&syncCalls==2,"invalid arguments never call controls");
    check(run("foo").lines[0]=="Unknown command: foo","unknown command");
    check(run("help").lines==std::vector<std::string>{"drawdebug <0|1>","vsync <0|1>","help","clear"},"help from allowlist");
    check(run("help x").lines[0]=="Usage: help"&&run("clear x").lines[0]=="Usage: clear","no-argument commands validated");
    check(run("clear").clearHistory&&hudCalls==2&&syncCalls==2,"clear has no runtime side effects");
    Display display;sf::RenderWindow unopened;DebugHud debug;
    RuntimeControls actual{[&](bool value){debug.enabled=value;},[&](bool value){display.setVerticalSync(unopened,value);}};
    executeConsoleCommand("vsync 1",actual);debug.update(.25,{},display.verticalSync());
    check(display.verticalSync()&&display.frameLimit()==0&&debug.performanceText().ends_with("VSYNC ON"),"real ON API and HUD");
    executeConsoleCommand("vsync 0",actual);debug.update(0,{},display.verticalSync());
    check(!display.verticalSync()&&display.frameLimit()==60&&debug.performanceText().ends_with("VSYNC OFF"),"real OFF API and HUD");
    executeConsoleCommand("drawdebug 1",actual);check(debug.enabled,"real HUD API");
    DevConsole console;
    const sf::Event grave=sf::Event::KeyPressed{.scancode=sf::Keyboard::Scan::Grave};
    auto type=[&](std::string_view text){for(unsigned char c:text)check(console.handleEvent(sf::Event::TextEntered{c},fake),"text consumed");};
    check(!console.handleEvent(key(sf::Keyboard::Key::A),fake),"closed leaves keys alone");
    check(console.handleEvent(grave,fake)&&console.isOpen(),"physical grave opens");
    type("~");check(console.inputLine().empty(),"opening text suppressed");
    type("helpX");console.handleEvent(key(sf::Keyboard::Key::Backspace),fake);check(console.inputLine()=="help","backspace");
    console.handleEvent(sf::Event::TextEntered{0xAC00},fake);check(console.inputLine()=="help","non ASCII ignored");
    for(auto c:{sf::Keyboard::Key::A,sf::Keyboard::Key::D,sf::Keyboard::Key::Space})check(console.handleEvent(key(c),fake),"game keys consumed");
    check(!console.handleEvent(key(sf::Keyboard::Key::Enter,true),fake),"Alt Enter passes to Display");
    check(!console.handleEvent(key(sf::Keyboard::Key::F12),fake)&&!console.handleEvent(key(sf::Keyboard::Key::F3),fake),"F12/F3 pass through");
    console.handleEvent(key(sf::Keyboard::Key::Enter),fake);check(console.history().size()==5&&console.history()[0]=="> help","echo and command execution");
    type("clear");console.handleEvent(key(sf::Keyboard::Key::Enter),fake);check(console.history().empty()&&hudCalls==2&&syncCalls==2,"clear removes only records");
    console.handleEvent(key(sf::Keyboard::Key::Escape),fake);check(!console.isOpen(),"escape closes");
    console.handleEvent(grave,fake);type("`");console.handleEvent(grave,fake);check(!console.isOpen(),"grave closes");
    for(int c=32;c<=126;++c){check(PixelFont::hasGlyph(static_cast<char>(c)),"ASCII glyph present");const auto rows=PixelFont::glyph(static_cast<char>(c));if(c!=32)check(std::any_of(rows.begin(),rows.end(),[](auto row){return row!=0;}),"ASCII ink present");}
    for(auto size:{sf::Vector2u{800,600},{1920,1080},{400,300},{200,150}}){
        sf::RenderTexture target(size);sf::View view(sf::FloatRect({100,-200},{800,450}));view.setViewport({{.1f,.1f},{.8f,.8f}});target.setView(view);
        const sf::Color clear(80,90,100);target.clear(clear);console.render(target);target.display();const auto before=target.getTexture().copyToImage();
        for(unsigned y=0;y<size.y;++y)for(unsigned x=0;x<size.x;++x)check(before.getPixel({x,y})==clear,"closed identical");
        check(target.getView().getCenter()==view.getCenter()&&target.getView().getViewport()==view.getViewport(),"closed view intact");
        console.handleEvent(grave,fake);type("~");type(std::string(300,'a'));check(console.inputLine().size()==256,"bounded input");
        console.render(target);target.display();const auto image=target.getTexture().copyToImage();const auto panel=DevConsole::panelBounds(size),input=DevConsole::inputBounds(size);
        check(input.position.x>=0&&input.position.y>=0&&input.position.x+input.size.x<=size.x&&input.position.y+input.size.y<=size.y,"input inside window");
        check(target.getView().getCenter()==view.getCenter()&&target.getView().getSize()==view.getSize()&&target.getView().getViewport()==view.getViewport(),"view restored");
        unsigned changed=0,inputInk=0;for(unsigned y=0;y<size.y;++y)for(unsigned x=0;x<size.x;++x){if(image.getPixel({x,y})!=clear){++changed;check(panel.contains({float(x)+.5f,float(y)+.5f}),"only panel changed");}if(input.contains({float(x)+.5f,float(y)+.5f})&&image.getPixel({x,y}).r>200)++inputInk;}
        check(changed>0&&inputInk>0,"panel and clipped input drawn");console.handleEvent(key(sf::Keyboard::Key::Escape),fake);
    }
    std::cout<<"PASS command table, fake/real controls, input routing, ASCII and 4 render sizes\n";
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
