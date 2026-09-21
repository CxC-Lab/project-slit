#include "DebugHud.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string_view>

namespace {
constexpr std::string_view characters="0123456789FPSWORLDXYms|.- ";
// Five bits per row, seven rows; only HUD characters, no font dependency.
constexpr std::array<std::array<unsigned,7>,24> glyphs{{
 {{14,17,19,21,25,17,14}},{{4,12,4,4,4,4,14}},{{14,17,1,2,4,8,31}},
 {{30,1,1,14,1,1,30}},{{2,6,10,18,31,2,2}},{{31,16,16,30,1,1,30}},
 {{14,16,16,30,17,17,14}},{{31,1,2,4,8,8,8}},{{14,17,17,14,17,17,14}},
 {{14,17,17,15,1,1,14}},{{31,16,16,30,16,16,16}},{{30,17,17,30,16,16,16}},
 {{15,16,16,14,1,1,30}},{{17,17,17,21,21,21,10}},{{14,17,17,17,17,17,14}},
 {{30,17,17,30,20,18,17}},{{16,16,16,16,16,16,31}},{{30,17,17,17,17,17,30}},
 {{17,17,10,4,10,17,17}},{{17,17,10,4,4,4,4}},{{0,0,26,21,21,21,21}},
 {{0,0,15,16,14,1,30}},{{4,4,4,4,4,4,4}},{{0,0,0,0,0,12,12}}
}};
std::array<unsigned,7> glyph(char c) {
    if(c=='-')return {0,0,0,31,0,0,0};
    if(c==' ')return {};
    const auto index=characters.find(c);return index<glyphs.size()?glyphs[index]:std::array<unsigned,7>{};
}
std::string fixed(double value,int decimals) {
    // Round before formatting so values rounded to zero never display a minus sign.
    const double factor=std::pow(10.,decimals);value=std::round(value*factor)/factor;
    if(value==0)value=0;
    std::ostringstream text;text.imbue(std::locale::classic());text<<std::fixed<<std::setprecision(decimals)<<value;return text.str();
}
}
bool DebugHud::hasGlyph(char c){return characters.find(c)!=std::string_view::npos;}
void DebugHud::update(double seconds,sf::Vector2f feet) {
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
    const auto width=std::max(performance_.size(),world_.size())*6u+7u;
    while(result>1&&(width*result+16u>pixels.x||24u*result+16u>pixels.y))--result;
    return result;
}
sf::FloatRect DebugHud::panelBounds(sf::Vector2u pixels) const {
    const auto factor=scale(pixels);
    const float x=std::min(8.f,static_cast<float>(pixels.x)),y=std::min(8.f,static_cast<float>(pixels.y));
    return {{x,y},{std::min(static_cast<float>((std::max(performance_.size(),world_.size())*6u+7u)*factor),pixels.x-x),
                    std::min(24.f*factor,pixels.y-y)}};
}
void DebugHud::render(sf::RenderTarget& target) const {
    if(!enabled)return;
    const auto size=target.getSize();if(!size.x||!size.y)return;
    const auto original=target.getView();
    const auto bounds=panelBounds(size);const float factor=static_cast<float>(scale(size));
    sf::VertexArray ink(sf::PrimitiveType::Triangles);
    unsigned row=0;
    for(const auto* text:{&performance_,&world_}){
        unsigned column=0;
        for(char c:*text){const auto bits=glyph(c);
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
