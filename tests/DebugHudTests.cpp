#include "DebugHud.hpp"
#include <iostream>
#include <stdexcept>
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool sameView(const sf::View& a,const sf::View& b){return a.getCenter()==b.getCenter()&&a.getSize()==b.getSize()&&a.getRotation()==b.getRotation()&&a.getViewport()==b.getViewport()&&a.getScissor()==b.getScissor();}
int main() try {
    DebugHud hud;
    for(int i=0;i<24;++i)hud.update(.01,{8742,-918.5f});
    check(hud.performanceText()=="FPS 0.0  |  0.00 ms","no refresh before quarter second");
    hud.update(.01,{8742,-918.5f});
    check(hud.performanceText()=="FPS 100.0  |  10.00 ms","fixed average formatting");
    check(hud.worldText()=="WORLD X 8742.0  Y -918.5","feet coordinate formatting");
    hud.update(.001,{-0.01f,-0.0f});check(hud.worldText()=="WORLD X 0.0  Y 0.0","negative zero removed");
    DebugHud mixed;mixed.update(.1,{});for(int i=0;i<40;++i)mixed.update(.01,{});
    // Forty-one frames / .50 seconds, not the most recent inverse.
    check(mixed.performanceText()=="FPS 82.0  |  12.20 ms","long frame diluted across window");
    for(int i=0;i<75;++i)mixed.update(.01,{});
    check(mixed.performanceText()=="FPS 100.0  |  10.00 ms","old samples expire from rolling window");
    for(char c:std::string("0123456789FPSWORLDXYms|.- "))check(DebugHud::hasGlyph(c),"required glyph exists");
    check(!DebugHud::hasGlyph('?'),"unsupported glyph detected");
    for(auto size:{sf::Vector2u{800,600},{1920,1080},{2560,1440},{400,300}}){
        sf::RenderTexture target(size);sf::View camera(sf::FloatRect({100,-400},{800,450}));
        camera.setViewport({{.1f,.1f},{.8f,.8f}});camera.setRotation(sf::degrees(13));target.setView(camera);
        const sf::Color clear(80,90,100);target.clear(clear);hud.enabled=false;hud.render(target);target.display();
        auto disabled=target.getTexture().copyToImage();
        check(sameView(target.getView(),camera),"disabled view unchanged");
        for(unsigned y=0;y<size.y;++y)for(unsigned x=0;x<size.x;++x)check(disabled.getPixel({x,y})==clear,"disabled pixel identical");
        hud.enabled=true;hud.update(0,{8742,-918.5f});hud.render(target);target.display();const auto first=target.getTexture().copyToImage();
        check(sameView(target.getView(),camera),"enabled view restored");
        const auto panel=hud.panelBounds(size);
        check(panel.position.x>=0&&panel.position.y>=0&&panel.position.x+panel.size.x<=size.x&&panel.position.y+panel.size.y<=size.y,"panel inside window");
        unsigned changed=0,bright=0;
        for(unsigned y=0;y<size.y;++y)for(unsigned x=0;x<size.x;++x)if(first.getPixel({x,y})!=clear){
            ++changed;check(panel.contains({float(x)+.5f,float(y)+.5f}),"HUD changes only panel pixels");bright+=first.getPixel({x,y}).r>200;
        }
        check(changed>0&&bright>0,"panel and glyphs drawn");
        camera.setCenter({-9000,4000});camera.setSize({1100,800});target.setView(camera);target.clear(clear);hud.render(target);target.display();
        const auto second=target.getTexture().copyToImage();check(sameView(target.getView(),camera),"other camera restored");
        for(unsigned y=0;y<size.y;++y)for(unsigned x=0;x<size.x;++x)check(first.getPixel({x,y})==second.getPixel({x,y}),"camera-independent pixels");
        hud.update(0,{123,456});target.clear(clear);hud.render(target);target.display();const auto moved=target.getTexture().copyToImage();
        bool difference=false;for(unsigned y=0;y<size.y;++y)for(unsigned x=0;x<size.x;++x)difference|=moved.getPixel({x,y})!=second.getPixel({x,y});
        check(difference,"position changes rendered text");
        std::cout<<"PASS pixels/view: "<<size.x<<'x'<<size.y<<'\n';
    }
    std::cout<<"PASS rolling average, refresh interval, formatting and glyph coverage\n";
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
