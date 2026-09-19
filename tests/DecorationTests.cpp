#include "Decorations.hpp"
#include "levels/Region.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
using J=nlohmann::json;
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main() try {
    namespace fs=std::filesystem;
    auto root=fs::current_path();
    while(!fs::exists(root/"assets/regions/avatar_lake.json")){
        check(root!=root.root_path(),"Cannot locate repository");root=root.parent_path();
    }
    const Region region(root/"assets/regions/avatar_lake.json");
    check(region.decorationFile().has_value(),"region default exists");
    Decorations real(*region.decorationFile());
    // Approved placements in draw order (ADR 0012). A missing, added, moved, relayered, reflipped or reordered
    // item fails here; per-item runtime path, approved provenance and hash are checked by approved_decoration_checks.
    const std::vector<std::tuple<std::string,int,int,std::string,bool>> approved{
        {"fossil01_ammonite",275,501,"behind",false},{"fossil01_ammonite",886,541,"behind",true},
        {"fossil02_relic",3100,541,"behind",false},{"fossil02_relic",9120,-809,"behind",true},
        {"fossil03_fragments",2000,541,"behind",false},{"fossil03_foreground_x2000",2000,536,"above",false},
        {"fossil03_fragments",5600,541,"behind",true},{"fossil03_foreground_x5600",5600,536,"above",false},
        {"rock_chip_a",916,523,"above",false},{"rock_chip_b",9523,-1200,"above",true},{"rock_boulder",8250,-1255,"behind",false},
        {"vine_mid",876,454,"above",false},{"vine_short",1062,434,"above",true},
        {"vine_long",352,138,"above",false},{"vine_mid",232,244,"above",true}};
    std::ifstream actual(*region.decorationFile());const auto items=J::parse(actual).at("items");
    check(items.size()==approved.size(),"approved default item count");
    for(std::size_t i=0;i<approved.size();++i){
        const auto& [name,x,y,layer,flip]=approved[i];const auto& item=items[i];
        check(item.at("image").get<std::string>()=="avatar_lake/runtime/"+name+".png","approved image in draw order");
        check(item.at("position")[0].get<double>()==x&&item.at("position")[1].get<double>()==y,"approved position");
        check(item.at("layer").get<std::string>()==layer&&item.at("flipX").get<bool>()==flip,"approved layer and flip");
    }
    const auto selection=selectDecorations(region.decorationFile(),"",false);
    check(selection.file==region.decorationFile()&&std::string(selection.mode)=="region_default","default rule");
    check(!selectDecorations(region.decorationFile(),"none",false).file,"none disables default");
    check(!selectDecorations(region.decorationFile(),"",true).file,"tileset override disables default");
    const auto explicitFile=selectDecorations(region.decorationFile(),"chosen.json",true);
    check(explicitFile.file==fs::path("chosen.json")&&std::string(explicitFile.mode)=="override","explicit file beats tileset override");
    check(!selectDecorations({},"",false).file,"missing default disabled");
    const auto work=root/"build/decoration_tests"/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    fs::create_directories(work);
    const auto regionFixture=work/"region.json";
    J regionData={{"id","fixture"},{"bounds",{0,0,800,600}},{"spawn",{100,100}},{"solids",J::array({{0,520,800,80}})}};
    {std::ofstream output(regionFixture);output<<regionData;}
    check(!Region(regionFixture).decorationFile(),"region without optional key");
    regionData["decorations"]="../escape";
    {std::ofstream output(regionFixture);output<<regionData;}
    bool invalidName=false;try{Region invalid(regionFixture);}catch(const std::exception&){invalidName=true;}
    check(invalidName,"invalid region decoration name rejected");

    sf::Image a(sf::Vector2u{4,4},sf::Color::Red);for(unsigned y=0;y<4;++y)a.setPixel({3,y},sf::Color::Green);
    check(a.saveToFile(work/"a.png"),"a fixture");
    check(sf::Image(sf::Vector2u{4,4},sf::Color::Blue).saveToFile(work/"b.png"),"b fixture");
    auto item=[](const char* image,const char* layer,bool flip){return J{{"image",image},{"position",{8,8}},{"layer",layer},{"flipX",flip}};};
    J data={{"items",J::array({item("a.png","above",false),item("b.png","above",false),item("a.png","above",true),item("b.png","behind",false)})}};
    auto outside=item("a.png","above",false);outside["position"]={100,100};data["items"].push_back(outside);
    const auto file=work/"set.json";
    auto write=[&](const J& j){std::ofstream out(file);out<<j;};write(data);
    Decorations decorations(file);
    check(decorations.captureIdentity().at("decorations").at("hash").get<std::string>().size()==16,"fixture capture hash");sf::RenderTexture target(sf::Vector2u{20,20});target.setView(sf::View(sf::FloatRect({0,0},{20,20})));
    target.clear(sf::Color::Transparent);
    check(decorations.render(target,Decorations::Layer::Behind)==1,"behind submitted count");target.display();
    check(target.getTexture().copyToImage().getPixel({6,4})==sf::Color::Blue,"behind layer");
    sf::RectangleShape terrain({20,20});terrain.setFillColor(sf::Color::Yellow);target.draw(terrain);
    check(decorations.render(target,Decorations::Layer::Above)==3,"offscreen excluded and file entries retained");target.display();
    const auto image=target.getTexture().copyToImage();
    check(image.getPixel({6,4})==sf::Color::Green&&image.getPixel({9,4})==sf::Color::Red,"A/B/A file order and horizontal flip");
    check(image.getPixel({5,4})==sf::Color::Yellow&&image.getPixel({6,8})==sf::Color::Yellow,"bottom center anchor and unit scale");
    target.setView(sf::View(sf::FloatRect({200,200},{20,20})));
    check(decorations.render(target,Decorations::Layer::Above)==0,"offscreen creates no submissions");
    for(int test=0;test<4;++test){auto invalid=data;
        if(test==0)invalid["items"][0]["layer"]="other";
        if(test==1)invalid["items"][0]["position"]="invalid";
        if(test==2)invalid["items"][0]["flipX"]="yes";
        if(test==3)invalid["items"][0]["image"]="missing.png";
        write(invalid);bool rejected=false;try{Decorations bad(file);}catch(const std::exception&){rejected=true;}
        check(rejected,"malformed decoration rejected");
    }
    std::cout<<"PASS: load, metadata, culling, layer/file order, flip and unit anchor\n";
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
