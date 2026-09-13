#include "TerrainTiles.hpp"
#include "levels/Region.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <chrono>
#include <nlohmann/json.hpp>
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

void macroChecks(const std::filesystem::path& avatarFile,const Terrain::Tileset& atlasSet)
{
    namespace fs=std::filesystem;
    const auto root=avatarFile.parent_path().parent_path().parent_path();
    const auto scratch=root/"build/preview/inner_macro_trial/tests"/
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    fs::create_directories(scratch);
    std::ifstream input(avatarFile.parent_path().parent_path()/"tilesets/avatar_lake_debug.json");
    auto data=nlohmann::json::parse(input);
    data["image"]=fs::absolute(atlasSet.image).generic_string();
    data["sampling"]={{"inner",{{"mode","repeat2d"},{"image","macro.png"}}}};
    const auto config=scratch/"tileset.json";
    {std::ofstream out(config);out<<data;}
    // Non-80, nonsquare image proves the repeat period is read from the texture.
    sf::Image image({7,5},sf::Color::Black);
    for(unsigned y=0;y<5;++y) for(unsigned x=0;x<7;++x)
        image.setPixel({x,y},sf::Color(static_cast<std::uint8_t>(30+x*20),static_cast<std::uint8_t>(40+y*30),75));
    check(image.saveToFile(scratch/"macro.png"),"write synthetic macro");
    const Terrain::Tileset macroSet(config);
    const std::vector<sf::FloatRect> solids{{{-100,-100},{80,100}},{{-20,-100},{80,100}},
                                          {{200,-60},{80,100}},{{320,0},{80,20}}};
    const auto groups=Terrain::components(solids);
    const sf::FloatRect visible{{-120,-120},{600,300}};
    const auto before=Terrain::pieces(groups,atlasSet,123,visible);
    const auto after=Terrain::pieces(groups,macroSet,123,visible);
    check(before.size()==after.size(),"macro does not change piece count");
    std::array<bool,10> inspected{};
    for(std::size_t i=0;i<after.size();++i) {
        const auto& a=before[i];const auto& b=after[i];inspected[b.role]=true;
        check(a.bounds==b.bounds && a.role==b.role && a.cell==b.cell,"macro preserves geometry and roles");
        check(b.uv.size==b.bounds.size,"macro one texel per world unit");
        if(b.sampling==Terrain::Sampling::Atlas)
            check(a.uv==b.uv && a.variant==b.variant,"nine atlas roles unchanged");
        else {
            const auto mid=b.bounds.position+b.bounds.size/2.f;
            const auto g=std::find_if(groups.begin(),groups.end(),[&](const auto& group){
                return std::any_of(group.solids.begin(),group.solids.end(),[&](const auto& r){return r.contains(mid);});});
            check(g!=groups.end() && b.uv.position==b.bounds.position-g->bounds.position,"independent signed component origins");
            check(b.variant==0,"one shared macro, no per-cell variants");
        }
    }
    check(std::all_of(inspected.begin(),inspected.end(),[](bool x){return x;}),"all ten roles exercised");
    const auto find=[&](sf::Vector2f at)->const Terrain::Piece& {
        const auto p=std::find_if(after.begin(),after.end(),[&](const auto& piece){return piece.bounds.contains(at);});
        check(p!=after.end(),"seam sample exists"); return *p;
    };
    const auto& left=find({-30,-60}); const auto& right=find({-10,-60});
    check(left.sampling==Terrain::Sampling::Repeat2D && right.sampling==left.sampling,"joined seam uses inner");
    check(right.uv.position-left.uv.position==right.bounds.position-left.bounds.position,"joined rectangles preserve phase");
    const sf::FloatRect moved{{-73,-77},{360,180}};
    for(const auto& b:Terrain::pieces(groups,macroSet,123,moved)) {
        const auto& a=find(b.bounds.position+b.bounds.size/2.f);
        check(b.uv.position==a.uv.position+b.bounds.position-a.bounds.position,"camera crop does not change UV phase");
    }
    TerrainTiles tiles(solids,config,123);
    sf::RenderTexture target({600,300});target.setView(sf::View(visible));
    target.clear();tiles.render(target,visible);target.display();
    check(target.getTexture().copyToImage().getPixel({60,60})==image.getPixel({40%7,40%5}),"GPU repeat uses image dimensions and nearest sampling");
    // After first render no image file is available; further renders must use the cache.
    fs::rename(scratch/"macro.png",scratch/"cached.png");
    target.clear();tiles.render(target,visible);target.display();
    check(target.getTexture().copyToImage().getPixel({60,60})==image.getPixel({40%7,40%5}),"macro loaded once across renders and components");
    for(bool corrupt:{false,true}) {
        if(corrupt) {std::ofstream bad(scratch/"macro.png");bad<<"not a PNG";}
        bool failed=false;
        try {TerrainTiles missing(solids,config,123);missing.render(target,visible);}
        catch(const std::exception& e) {failed=std::string(e.what()).find("Cannot load inner macro image:")!=std::string::npos;}
        check(failed,"missing/corrupt macro reports image path");
    }
    fs::create_directories(scratch/"regions");fs::create_directories(scratch/"tilesets");
    data["sampling"]["inner"]["image"]="../cached.png";
    {std::ofstream out(scratch/"tilesets/synthetic.json");out<<data;}
    {std::ofstream out(scratch/"regions/synthetic.json");
        out<<R"({"id":"synthetic","bounds":[0,0,800,600],"spawn":[100,100],"solids":[[0,520,800,80]]})";}
    Region normal(scratch/"regions/synthetic.json"),trial(scratch/"regions/synthetic.json","synthetic");
    check(normal.bounds()==trial.bounds() && normal.spawn()==trial.spawn() && normal.solids()==trial.solids(),"trial override preserves region facts");
    check(normal.captureIdentity().at("tilesetMode")=="region_default" && trial.captureIdentity().at("tileset")=="synthetic" &&
          trial.captureIdentity().at("tilesetMode")=="override" && !trial.captureIdentity().at("innerMacroHash").get<std::string>().empty(),"capture identity uses applied synthetic tileset");
    // Same region/name/launch arguments; only the default tileset data changes.
    {std::ofstream out(scratch/"regions/synthetic.json");
        out<<R"({"id":"synthetic","tileset":"synthetic","bounds":[0,0,800,600],"spawn":[100,100],"solids":[[0,520,800,80]]})";}
    Region firstDefault(scratch/"regions/synthetic.json");
    sf::Image nextImage({7,5},sf::Color(12,34,56));
    check(nextImage.saveToFile(scratch/"next.png"),"write next candidate fixture");
    data["sampling"]["inner"]["image"]="../next.png";
    {std::ofstream out(scratch/"tilesets/synthetic.json");out<<data;}
    Region nextDefault(scratch/"regions/synthetic.json");
    check(firstDefault.tilesetName()==nextDefault.tilesetName() &&
          firstDefault.captureIdentity().at("innerMacroHash")!=nextDefault.captureIdentity().at("innerMacroHash"),
          "new default candidate needs data only, not new launch arguments");
    target.setView(sf::View(sf::FloatRect({0,500},{600,300})));
    target.clear();firstDefault.render(target);target.display();
    const auto firstPixel=target.getTexture().copyToImage().getPixel({40,60});
    target.clear();nextDefault.render(target);target.display();
    check(firstPixel!=target.getTexture().copyToImage().getPixel({40,60}) &&
          target.getTexture().copyToImage().getPixel({40,60})==sf::Color(12,34,56),"data-only default replacement is rendered");
    std::cout<<"PASS: macro continuity, signed origins, camera invariance, atlas preservation, cache and image failures\n";
}


void macroCaptures(const std::filesystem::path& planFile,const std::filesystem::path& output)
{
    namespace fs=std::filesystem;
    const auto read=[](const fs::path& path) {
        std::ifstream in(path);check(static_cast<bool>(in),"capture JSON opens");
        return nlohmann::json::parse(in);
    };
    const auto plan=read(planFile);
    const auto file=Region::findFile(plan.at("region").get<std::string>());
    const Region room(file);
    const auto groups=Terrain::components(room.solids());
    const auto diagnostic=output/plan.at("baseline").get<std::string>();
    const auto baselineMetadata=read(diagnostic/"manifest.json");
    const auto pair=plan.at("cameraPair").get<std::array<unsigned,2>>();
    nlohmann::json report{{"plan",plan},{"trials",nlohmann::json::array()}};
    for(const auto& trial:plan.at("trials")) {
        const auto id=trial.at("id").get<std::string>();
        const auto name=trial.at("tileset").get<std::string>();
        const auto dir=output/id;
        const auto metadata=read(dir/"manifest.json");
        const Region applied(file,name);
        check(metadata.at("region")==plan.at("region"),"capture region identity");
        const auto identity=applied.captureIdentity();
        for(const auto& [key,value]:identity.items())
            check(metadata.at(key)==value,"actual capture tileset identity");
        check(metadata.at("cells").size()==plan.at("points").size(),"all planned points captured");
        const auto setFile=file.parent_path().parent_path()/"tilesets"/(applied.tilesetName()+".json");
        const Terrain::Tileset set(setFile);
        sf::Image tile;
        if(set.innerImage) check(tile.loadFromFile(*set.innerImage),"candidate macro loads");
        std::size_t samples=0,differences=0,edgeSamples=0,edgeDifferences=0,overlap=0,shiftDifferences=0;
        for(std::size_t i=0;i<metadata.at("cells").size();++i) {
            const auto& cell=metadata.at("cells")[i];const auto& rect=cell.at("visibleRect");
            const sf::Vector2f lo{rect.at("left").get<float>(),rect.at("top").get<float>()};
            sf::Image image,base,moved;
            check(image.loadFromFile(dir/cell.at("file").get<std::string>()),"candidate capture loads");
            check(base.loadFromFile(diagnostic/baselineMetadata.at("cells")[i].at("file").get<std::string>()),"baseline capture loads");
            check(image.getSize()==base.getSize() && cell.at("cameraCenter")==baselineMetadata.at("cells")[i].at("cameraCenter"),"baseline shares composition");
            sf::Vector2f delta{};
            if(i==pair[0]) {
                const auto& other=metadata.at("cells").at(pair[1]);
                delta={other.at("visibleRect").at("left").get<float>()-lo.x,
                       other.at("visibleRect").at("top").get<float>()-lo.y};
                check(std::abs(delta.x-std::round(delta.x))<.001f && std::abs(delta.y-std::round(delta.y))<.001f,"comparison uses integer world shift");
                check(moved.loadFromFile(dir/other.at("file").get<std::string>()),"shifted capture loads");
            }
            const sf::FloatRect visible{lo,{rect.at("width").get<float>(),rect.at("height").get<float>()}};
            for(const auto& piece:Terrain::pieces(groups,set,123,visible)) {
                const bool macro=piece.sampling==Terrain::Sampling::Repeat2D;
                for(int y=std::max(0,static_cast<int>(std::ceil(piece.bounds.position.y-lo.y-.5f)));
                    y<std::min(static_cast<int>(image.getSize().y),static_cast<int>(std::ceil(piece.bounds.position.y+piece.bounds.size.y-lo.y-.5f)));++y)
                for(int x=std::max(0,static_cast<int>(std::ceil(piece.bounds.position.x-lo.x-.5f)));
                    x<std::min(static_cast<int>(image.getSize().x),static_cast<int>(std::ceil(piece.bounds.position.x+piece.bounds.size.x-lo.x-.5f)));++x) {
                    const sf::Vector2u pixel{static_cast<unsigned>(x),static_cast<unsigned>(y)};
                    if(!macro) {++edgeSamples;edgeDifferences+=image.getPixel(pixel)!=base.getPixel(pixel);continue;}
                    const auto uv=piece.uv.position+lo+sf::Vector2f{x+.5f,y+.5f}-piece.bounds.position;
                    const sf::Vector2u at{static_cast<unsigned>(std::floor(uv.x)),static_cast<unsigned>(std::floor(uv.y))};
                    ++samples;
                    differences+=image.getPixel(pixel)!=tile.getPixel({at.x%tile.getSize().x,at.y%tile.getSize().y});
                    if(i==pair[0]) {
                        const int mx=x-static_cast<int>(std::round(delta.x)),my=y-static_cast<int>(std::round(delta.y));
                        if(mx>=0 && my>=0 && mx<static_cast<int>(moved.getSize().x) && my<static_cast<int>(moved.getSize().y)) {
                            ++overlap;shiftDifferences+=image.getPixel(pixel)!=moved.getPixel({static_cast<unsigned>(mx),static_cast<unsigned>(my)});
                        }
                    }
                }
            }
        }
        check(differences==0 && edgeDifferences==0 && shiftDifferences==0,"candidate sampling, baseline and camera invariants");
        if(set.innerImage) check(samples>0 && overlap>0,"macro checks have samples");
        report["trials"].push_back({{"id",id},{"macroSamples",samples},{"macroDifferences",differences},
            {"nonInnerSamples",edgeSamples},{"nonInnerDifferences",edgeDifferences},
            {"cameraOverlapSamples",overlap},{"cameraDifferences",shiftDifferences}});
    }
    std::ofstream out(output/"verification.json");out<<report.dump(2)<<'\n';
    check(static_cast<bool>(out),"verification report write");
    std::cout<<report.dump(2)<<'\n';
}

int main(int argc,char** argv) try
{
    if(argc==4 && std::string(argv[1])=="--macro-captures") {
        macroCaptures(argv[2],argv[3]);return 0;
    }
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
    const Terrain::Tileset set(avatarFile.parent_path().parent_path()/"tilesets/avatar_lake_debug.json");
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
    macroChecks(avatarFile,set);
    sf::RenderTexture target({320,240});
    Region practice(Region::findFile("practice_room"));
    target.clear(); practice.render(target); target.display(); // Missing tileset keeps the old render path.
    std::cout<<"PASS: terrain union, roles, clipping, variants, culling and fallback\n";
}
catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
