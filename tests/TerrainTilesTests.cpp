#include "TerrainTiles.hpp"
#include "levels/Region.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
double area(const std::vector<sf::FloatRect>& rectangles)
{
    std::vector<float> xs;
    for(const auto& r:rectangles) {xs.push_back(r.position.x);xs.push_back(r.position.x+r.size.x);}
    std::sort(xs.begin(),xs.end()); xs.erase(std::unique(xs.begin(),xs.end()),xs.end());
    double total=0;
    for(std::size_t i=1;i<xs.size();++i) {
        std::vector<sf::Vector2f> intervals;
        for(const auto& r:rectangles) if(r.position.x<xs[i] && r.position.x+r.size.x>xs[i-1])
            intervals.push_back({r.position.y,r.position.y+r.size.y});
        std::sort(intervals.begin(),intervals.end(),[](auto a,auto b){return a.x<b.x;});
        float end=-1e30f;
        for(auto v:intervals) {total+=(xs[i]-xs[i-1])*std::max(0.f,v.y-std::max(end,v.x));end=std::max(end,v.y);}
    }
    return total;
}
int main(int argc,char** argv) try
{
    if(argc==4 && std::string(argv[1])=="--compare") {
        sf::Image game,preview;
        check(game.loadFromFile(argv[2]) && preview.loadFromFile(argv[3]),"comparison images load");
        const sf::Color colors[]{ {80,95,120},{50,170,90},{150,90,180},{50,135,200},{205,130,50},
            {160,190,65},{55,190,180},{200,80,120},{190,150,170},{160,120,60},sf::Color::White };
        unsigned samples=0,matches=0;
        for(unsigned y=1;y+1<preview.getSize().y;++y) for(unsigned x=1;x+1<preview.getSize().x;++x) {
            const auto color=preview.getPixel({x,y});
            if(std::find(std::begin(colors),std::end(colors),color)==std::end(colors)) continue;
            if(preview.getPixel({x-1,y})!=color || preview.getPixel({x+1,y})!=color ||
               preview.getPixel({x,y-1})!=color || preview.getPixel({x,y+1})!=color) continue;
            const sf::Vector2u at{static_cast<unsigned>((x+.5)*game.getSize().x/preview.getSize().x),
                                 static_cast<unsigned>((y+.5)*game.getSize().y/preview.getSize().y)};
            ++samples; matches+=game.getPixel(at)==color;
        }
        check(samples>100 && static_cast<double>(matches)/samples>.999,"game/preview terrain pixels agree");
        std::cout<<"Game/preview terrain interior pixels: "<<matches<<'/'<<samples<<'\n';
        return 0;
    }
    const auto avatarFile=Region::findFile("avatar_lake");
    const Terrain::Tileset set(avatarFile.parent_path().parent_path()/"tilesets/avatar_lake.json");
    sf::Image atlas;check(atlas.loadFromFile(set.image),"diagnostic image load");
    check(atlas.getSize()==sf::Vector2u(*std::max_element(set.variants.begin(),set.variants.end())*set.size,
                                     static_cast<unsigned>(Terrain::roles.size())*set.size),"atlas dimensions");
    for(unsigned y=0;y<atlas.getSize().y;++y) for(unsigned x=0;x<atlas.getSize().x;++x)
        check(atlas.getPixel({x,y}).a==0 || atlas.getPixel({x,y}).a==255,"binary alpha");
    for(const auto name:{"avatar_lake","practice_room"}) {
        Region room(Region::findFile(name));
        const auto groups=Terrain::components(room.solids());
        const bool avatar=std::string(name)=="avatar_lake";
        check(groups.size()==(avatar?18:10),"component count");
        const auto largest=std::max_element(groups.begin(),groups.end(),[](const auto& a,const auto& b){return a.solids.size()<b.solids.size();});
        check(largest->solids.size()==(avatar?14:7),"largest component membership");
        check(largest->bounds==sf::FloatRect({0,avatar?-4650.f:-1050.f},{avatar?9600.f:3200.f,avatar?5250.f:1650.f}),"largest component bounds");
        const auto all=Terrain::pieces(groups,set,123,room.bounds());
        double total=0;
        for(const auto& p:all) {
            total+=p.bounds.size.x*p.bounds.size.y;
            std::vector<sf::FloatRect> intersections;
            for(const auto& solid:room.solids()) if(auto r=solid.findIntersection(p.bounds)) intersections.push_back(*r);
            check(std::abs(area(intersections)-p.bounds.size.x*p.bounds.size.y)<.001,"piece entirely inside union");
            bool phase=false;
            for(const auto& g:groups) if(g.bounds.contains(p.bounds.position+p.bounds.size/2.f)) {
                const auto q=(p.cell-g.bounds.position)/static_cast<float>(set.size);
                if(std::abs(q.x-std::round(q.x))<.001 && std::abs(q.y-std::round(q.y))<.001) phase=true;
            }
            check(phase,"component common grid origin");
            check(p.uv.size==p.bounds.size,"one texel per world unit");
            const auto local=p.uv.position-sf::Vector2f{static_cast<float>(p.variant*set.size),static_cast<float>(p.role*set.size)};
            check(local.x>=0 && local.y>=0 && local.x+p.uv.size.x<=set.size+.001f && local.y+p.uv.size.y<=set.size+.001f,"edge clipping stays inside selected tile");
        }
        check(std::abs(total-area(room.solids()))<.001,"tile union area equals solid union");
        std::cout<<name<<": components="<<groups.size()<<" largest="<<largest->solids.size()<<" area="<<total<<'\n';
    }
    const float t=static_cast<float>(set.size);
    const std::vector<sf::FloatRect> joined{{{0,0},{2*t,3*t}},{{2*t,0},{2*t,3*t}}};
    const auto group=Terrain::components(joined);
    const sf::FloatRect visible{{0,0},{4*t,3*t}};
    const auto a=Terrain::pieces(group,set,1,visible),b=Terrain::pieces(group,set,1,visible),c=Terrain::pieces(group,set,2,visible);
    bool changed=false,seam=false;
    check(a.size()==b.size() && a.size()==c.size(),"stable geometry");
    for(std::size_t i=0;i<a.size();++i) {
        check(a[i].bounds==b[i].bounds && a[i].uv==b[i].uv,"deterministic variants");
        changed|=a[i].variant!=c[i].variant;
        if(a[i].cell==sf::Vector2f{2*t,t}) {check(a[i].role==0,"joined rectangle seam is inner");seam=true;}
    }
    check(changed && seam,"seed affects variants and seam was inspected");
    const auto slab=Terrain::pieces(Terrain::components({{{8,10},{4*t,t}}}),set,1,{{0,0},{8*t,8*t}});
    for(const auto& p:slab) check(p.role==9,"single-height platform is slab");
    const sf::FloatRect crop{{t+.5f,t+.5f},{t,t}};
    for(const auto& p:Terrain::pieces(group,set,1,crop))
        check(p.bounds.findIntersection(crop)==p.bounds,"no offscreen submission");
    check(Terrain::pieces(group,set,1,{{10000,10000},{t,t}}).empty(),"offscreen component culled");
    check(Terrain::components({{{0,0},{t,t}},{{t,t},{t,t}}}).size()==2,"point contacts remain separate");
    // Nonaligned overlapping solids: per-pixel coverage must be exactly once, including cuts.
    const std::vector<sf::FloatRect> irregular{{{0,0},{t+4,t+7}},{{t-3,5},{t+2,t+9}}};
    const auto mesh=Terrain::pieces(Terrain::components(irregular),set,1,{{0,0},{4*t,4*t}});
    for(float y=.5f;y<3*t;y+=1) for(float x=.5f;x<3*t;x+=1) {
        const sf::Vector2f p{x,y};int count=0;
        for(const auto& piece:mesh) count+=piece.bounds.contains(p);
        const bool covered=std::any_of(irregular.begin(),irregular.end(),[&](auto r){return r.contains(p);});
        check(count==(covered?1:0),"union has no overlaps or missing pixels");
    }
    sf::RenderTexture target({320,240});
    Region practice(Region::findFile("practice_room"));
    target.clear(); practice.render(target); target.display(); // Missing tileset keeps the old render path.
    std::cout<<"PASS: terrain union, roles, clipping, variants, culling and fallback\n";
}
catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
