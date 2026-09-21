#include "DevConsole.hpp"
#include "PixelFont.hpp"
#include <algorithm>
namespace {
unsigned scale(sf::Vector2u size){return size.x<400||size.y<300?1u:std::max(2u,size.y/450u);}
bool passthrough(sf::Keyboard::Key code,bool alt){return code==sf::Keyboard::Key::F3||code==sf::Keyboard::Key::F12||(code==sf::Keyboard::Key::Enter&&alt);}
}
void DevConsole::record(const std::string& line){history_.push_back(line);while(history_.size()>64)history_.pop_front();}
bool DevConsole::handleEvent(const sf::Event& event,const RuntimeControls& controls){
    if(const auto* key=event.getIf<sf::Event::KeyPressed>()){
        if(key->scancode==sf::Keyboard::Scan::Grave){open_=!open_;input_.clear();suppressToggleText_=true;return true;}
        if(!open_||passthrough(key->code,key->alt))return false;
        if(key->code==sf::Keyboard::Key::Escape){open_=false;input_.clear();return true;}
        if(key->code==sf::Keyboard::Key::Backspace){if(!input_.empty())input_.pop_back();return true;}
        if(key->code==sf::Keyboard::Key::Enter){
            if(input_.find_first_not_of(" \t\r\n")!=std::string::npos){
                record("> "+input_);const auto result=executeConsoleCommand(input_,controls);
                if(result.clearHistory)history_.clear();
                for(const auto& line:result.lines)record(line);
            }input_.clear();return true;
        }
        return true;
    }
    if(const auto* key=event.getIf<sf::Event::KeyReleased>()){
        if(key->scancode==sf::Keyboard::Scan::Grave)return true;
        return open_&&!passthrough(key->code,key->alt);
    }
    if(const auto* text=event.getIf<sf::Event::TextEntered>()){
        if(suppressToggleText_){suppressToggleText_=false;if(text->unicode=='`'||text->unicode=='~')return true;}
        if(!open_)return false;
        if(text->unicode>=32&&text->unicode<=126&&input_.size()<256)input_.push_back(static_cast<char>(text->unicode));
        return true;
    }
    return false;
}
sf::FloatRect DevConsole::panelBounds(sf::Vector2u pixels){return {{0,0},{static_cast<float>(pixels.x),std::min(static_cast<float>(pixels.y),80.f*scale(pixels))}};}
sf::FloatRect DevConsole::inputBounds(sf::Vector2u pixels){
    const auto panel=panelBounds(pixels);const float factor=static_cast<float>(scale(pixels));
    return {{std::min(4.f*factor,panel.size.x),std::max(0.f,panel.size.y-11.f*factor)},
            {std::max(0.f,panel.size.x-8.f*factor),std::min(7.f*factor,panel.size.y)}};
}
void DevConsole::render(sf::RenderTarget& target) const {
    if(!open_)return;
    const auto size=target.getSize();if(!size.x||!size.y)return;
    const auto panel=panelBounds(size),input=inputBounds(size);const float factor=static_cast<float>(scale(size));
    const auto columns=static_cast<std::size_t>(input.size.x/(6.f*factor));
    sf::VertexArray ink(sf::PrimitiveType::Triangles);
    const auto text=[&](std::string_view line,sf::Vector2f origin){
        for(std::size_t c=0;c<std::min(columns,line.size());++c){const auto bits=PixelFont::glyph(line[c]);
            for(unsigned y=0;y<7;++y)for(unsigned x=0;x<5;++x)if(bits[y]&(1u<<(4-x))){
                const auto a=origin+sf::Vector2f{static_cast<float>(c*6+x)*factor,static_cast<float>(y)*factor},b=a+sf::Vector2f{factor,factor};
                if(b.x>panel.size.x||b.y>panel.size.y)continue;
                for(auto p:{a,sf::Vector2f{b.x,a.y},b,a,b,sf::Vector2f{a.x,b.y}})ink.append(sf::Vertex{p,sf::Color(235,240,245)});
            }
        }
    };
    const auto lines=static_cast<std::size_t>(std::max(0.f,(input.position.y-4.f*factor)/(9.f*factor)));
    const auto first=history_.size()>lines?history_.size()-lines:0;
    for(auto i=first;i<history_.size();++i)text(history_[i],{4.f*factor,(4.f+9.f*static_cast<float>(i-first))*factor});
    const auto keep=columns>3?columns-3:0;
    text("> "+input_.substr(input_.size()>keep?input_.size()-keep:0)+"_",input.position);
    sf::RectangleShape background(panel.size);background.setFillColor(sf::Color(0,0,0,215));
    const auto original=target.getView();target.setView(sf::View(sf::FloatRect({0,0},sf::Vector2f(size))));
    target.draw(background);target.draw(ink);target.setView(original);
}
