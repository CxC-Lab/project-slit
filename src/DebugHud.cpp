#include "DebugHud.hpp"
#include "PixelFont.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string_view>

namespace {
std::string fixed(double value,int decimals) {
    // Round before formatting so values rounded to zero never display a minus sign.
    const double factor=std::pow(10.,decimals);value=std::round(value*factor)/factor;
    if(value==0)value=0;
    std::ostringstream text;text.imbue(std::locale::classic());text<<std::fixed<<std::setprecision(decimals)<<value;return text.str();
}
}
bool DebugHud::hasGlyph(char c){return std::string_view("0123456789FPSWORLDXYms|.- VNC").find(c)!=std::string_view::npos;}
bool DebugHud::glyphHasInk(char c){const auto rows=PixelFont::glyph(c);return std::any_of(rows.begin(),rows.end(),[](auto row){return row!=0;});}
void DebugHud::update(double seconds,sf::Vector2f feet,bool vsync) {
    vsync_=vsync;
    world_="WORLD X "+fixed(feet.x,1)+"  Y "+fixed(feet.y,1);
    if(!std::isfinite(seconds)||seconds<=0)return;
    time_+=seconds;frames_.push_back({time_,seconds});total_+=seconds;
    // Keep frames completed in the last half-second; drop an entire expired sample.
    while(frames_.size()>1&&frames_.front().end<=time_-0.5+1e-9){total_-=frames_.front().seconds;frames_.pop_front();}
    if(time_-lastRefresh_+1e-9>=0.25){
        const double count=static_cast<double>(frames_.size());
        performance_="FPS "+fixed(count/total_,1)+"  |  "+fixed(1000.*total_/count,2)+" ms";
        lastRefresh_=time_;
    }
}
unsigned DebugHud::scale(sf::Vector2u pixels) const {
    unsigned result=std::max(2u,pixels.y/450u);
    const auto width=std::max(performanceText().size(),world_.size())*6u+7u;
    while(result>1&&(width*result+16u>pixels.x||24u*result+16u>pixels.y))--result;
    return result;
}
sf::FloatRect DebugHud::panelBounds(sf::Vector2u pixels) const {
    const auto factor=scale(pixels);
    const float x=std::min(8.f,static_cast<float>(pixels.x)),y=std::min(8.f,static_cast<float>(pixels.y));
    return {{x,y},{std::min(static_cast<float>((std::max(performanceText().size(),world_.size())*6u+7u)*factor),pixels.x-x),
                    std::min(24.f*factor,pixels.y-y)}};
}
void DebugHud::render(sf::RenderTarget& target) const {
    if(!enabled)return;
    const auto size=target.getSize();if(!size.x||!size.y)return;
    const auto original=target.getView();
    const auto bounds=panelBounds(size);const float factor=static_cast<float>(scale(size));
    sf::VertexArray ink(sf::PrimitiveType::Triangles);
    unsigned row=0;
    const auto performance=performanceText();
    for(const auto* text:{&performance,&world_}){
        unsigned column=0;
        for(char c:*text){const auto bits=PixelFont::glyph(c);
            for(unsigned y=0;y<7;++y)for(unsigned x=0;x<5;++x)if(bits[y]&(1u<<(4-x))){
                const auto a=bounds.position+sf::Vector2f{(4.f+static_cast<float>(column*6+x))*factor,(4.f+static_cast<float>(row*9+y))*factor};
                const auto b=a+sf::Vector2f{factor,factor};
                if(b.x>bounds.position.x+bounds.size.x||b.y>bounds.position.y+bounds.size.y)continue;
                for(auto p:{a,sf::Vector2f{b.x,a.y},b,a,b,sf::Vector2f{a.x,b.y}})ink.append(sf::Vertex{p,sf::Color(235,240,245)});
            }++column;
        }++row;
    }
    sf::RectangleShape panel(bounds.size);panel.setPosition(bounds.position);panel.setFillColor(sf::Color(0,0,0,180));
    target.setView(sf::View(sf::FloatRect({0,0},sf::Vector2f(size))));
    target.draw(panel);target.draw(ink);target.setView(original);
}
