#include "TerrainTiles.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) try
{
    if(argc!=2) throw std::runtime_error("Usage: tileset_generator <tileset.json>");
    const Terrain::Tileset set(argv[1]);
    const unsigned t=static_cast<unsigned>(set.size);
    const unsigned width=*std::max_element(set.variants.begin(),set.variants.end())*t;
    sf::Image image({width,static_cast<unsigned>(Terrain::roles.size())*t},sf::Color::Transparent);
    const sf::Color colors[]{ {80,95,120},{50,170,90},{150,90,180},{50,135,200},{205,130,50},
                              {160,190,65},{55,190,180},{200,80,120},{190,150,170},{160,120,60} };
    const unsigned border=std::max(1u,t/8),period=std::max(1u,t/4);
    for(unsigned role=0;role<Terrain::roles.size();++role) for(unsigned variant=0;variant<set.variants[role];++variant)
        for(unsigned y=0;y<t;++y) for(unsigned x=0;x<t;++x) {
            sf::Color color=colors[role];
            if(role==0 && (x%period==0 || y%period==0)) color={110,125,150};
            const bool top=role==1 || role==5 || role==6 || role==9;
            const bool bottom=role==2 || role==7 || role==8 || role==9;
            const bool left=role==3 || role==5 || role==7;
            const bool right=role==4 || role==6 || role==8;
            if((top && y<border)||(bottom && y>=t-border)||(left && x<border)||(right && x>=t-border))
                color=sf::Color::White;
            // Variant dots only in the interior: every inner variant has identical seams.
            if(y==t/2 && x>border && x<t-border && x%2==0 && x/2<=variant+1) color=sf::Color::Black;
            image.setPixel({variant*t+x,role*t+y},color);
        }
    if(!image.saveToFile(set.image)) throw std::runtime_error("Tileset save failed");
    std::cout<<set.image.string()<<" generated\n";
}
catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
