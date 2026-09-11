#include <SFML/Graphics/Image.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

// Procedural destination pixels only: no source image, sampling, resizing or rescaling.
namespace {
float distance(float x, float center) {
    float d=std::abs(x-center);
    return std::min(d,1600.f-d);
}
bool cloud(float x,float y,float cx,float cy) {
    const float dx=distance(x,cx);
    return dx*dx/(100.f*100.f)+(y-cy)*(y-cy)/(23.f*23.f)<1.f ||
           distance(x,cx-32)*distance(x,cx-32)/(47.f*47.f)+(y-cy+16)*(y-cy+16)/(34.f*34.f)<1.f;
}
int ridge(float x,bool distant) {
    const float d=std::min(distance(x,distant ? 220.f : 220.f),distance(x,distant ? 1010.f : 1110.f));
    // Flat tepui summit, steep flanks, then foothills. All heights are native destination pixels.
    const float summit=distant ? 430.f : 800.f;
    return static_cast<int>(summit + (d<125 ? 0 : d<170 ? (d-125)*1.9f : 85.5f+(d-170)*.18f));
}
}
int main(int argc,char** argv) try {
    if(argc!=2) { std::cerr<<"Usage: bg_generator <repository-root>\n"; return 1; }
    const auto output=std::filesystem::path(argv[1])/"assets/backgrounds/avatar_lake";
    std::filesystem::create_directories(output);
    constexpr unsigned width=1600;
    const unsigned heights[]{720,960,1260};
    const char* names[]{"sky.png","distant.png","middle.png"};
    for(int layer=0;layer<3;++layer) {
        sf::Image image({width,heights[layer]},sf::Color::Transparent);
        for(unsigned y=0;y<heights[layer];++y) for(unsigned x=0;x<width;++x) {
            sf::Color color=sf::Color::Transparent;
            if(layer==0) {
                const float t=static_cast<float>(y)/(heights[layer]-1);
                color=sf::Color(static_cast<unsigned char>(135+79*t),
                                static_cast<unsigned char>(185+46*t),
                                static_cast<unsigned char>(216+16*t));
                if(cloud(static_cast<float>(x),static_cast<float>(y),180,225) ||
                   cloud(static_cast<float>(x),static_cast<float>(y),700,320) ||
                   cloud(static_cast<float>(x),static_cast<float>(y),1270,180))
                    color=sf::Color(237,243,240);
            } else {
                const int top=ridge(static_cast<float>(x),layer==1);
                if(static_cast<int>(y)>=top) {
                    color=layer==1 ? sf::Color(145,172,170) : sf::Color(61,111,100);
                    if(static_cast<int>(y)<top+8)
                        color=layer==1 ? sf::Color(154,180,172) : sf::Color(83,134,100);
                    if(layer==2 && x%47<3) color=sf::Color(55,102,94);
                }
                if(layer==2) {
                    constexpr unsigned waterline=1005; // -840 + 375*(1-.12) + 1005 = world y 495.
                    if(static_cast<int>(y)>=top && y<waterline && (distance(static_cast<float>(x),320)<4 || distance(static_cast<float>(x),1190)<6))
                        color=sf::Color(180,218,217);
                    if(y>=waterline) {
                        color=sf::Color(76,148,165);
                        const float dx=static_cast<float>(x)+std::sin(y*.19f)*7.f;
                        // Baked broken reflection of the same tepui profile, not another render pass.
                        const int reflectedY=static_cast<int>(2*waterline-y);
                        if(reflectedY>=ridge(std::fmod(dx+1600.f,1600.f),false)) color=sf::Color(70,128,137);
                        if(y%19==0 && x%180<100) color=sf::Color(136,186,195);
                        if(y==waterline) color=sf::Color(162,206,211);
                    }
                }
            }
            image.setPixel({x,y},color);
        }
        for(unsigned y=0;y<heights[layer];++y) image.setPixel({width-1,y},image.getPixel({0,y}));
        if(!image.saveToFile(output/names[layer])) throw std::runtime_error("PNG save failed");
        std::cout<<names[layer]<<" "<<width<<"x"<<heights[layer]<<'\n';
    }
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
