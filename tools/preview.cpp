#include "Display.hpp"
#include "Background.hpp"
#include "levels/Region.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

sf::Vector2f coordinate(const std::string& text)
{
    const auto comma=text.find(',');
    if(comma==std::string::npos) throw std::runtime_error("Expected x,y: "+text);
    const auto number=[](const std::string& part) {
        std::size_t used=0;
        float value;
        try { value=std::stof(part,&used); }
        catch(const std::exception&) { throw std::runtime_error("Invalid coordinate: "+part); }
        if(used!=part.size() || !std::isfinite(value)) throw std::runtime_error("Invalid coordinate: "+part);
        return value;
    };
    return {number(text.substr(0,comma)),number(text.substr(comma+1))};
}

void diagnostic(sf::RenderTexture& target,const sf::View& view)
{
    const auto lo=view.getCenter()-view.getSize()/2.f, hi=lo+view.getSize();
    sf::VertexArray lines(sf::PrimitiveType::Lines);
    const auto line=[&](sf::Vector2f a,sf::Vector2f b) {
        lines.append(sf::Vertex{a,sf::Color(230,190,80)});
        lines.append(sf::Vertex{b,sf::Color(230,190,80)});
    };
    for(float x=std::ceil(lo.x/800.f)*800.f;x<hi.x;x+=800.f) line({x,lo.y},{x,hi.y});
    for(float y=std::ceil(lo.y/450.f)*450.f;y<hi.y;y+=450.f) line({lo.x,y},{hi.x,y});
    for(float x : {lo.x+2,hi.x-2}) for(float y : {lo.y+2,hi.y-2}) {
        line({x,y},{x+(x<view.getCenter().x ? 12.f : -12.f),y});
        line({x,y},{x,y+(y<view.getCenter().y ? 12.f : -12.f)});
    }
    target.draw(lines);
    sf::RectangleShape box({32,48});
    box.setPosition(view.getCenter()-box.getSize()/2.f);
    box.setFillColor(sf::Color::Transparent);
    box.setOutlineColor(sf::Color::Yellow);
    box.setOutlineThickness(-1);
    target.draw(box);
}

// Native-pixel sheet indices; no font asset and no scaling of the captured images.
void label(sf::Image& image,unsigned x,unsigned y,const std::string& text)
{
    const char* digits[]{"111101101101111","010110010010111","111001111100111",
        "111001111001111","101101111001001","111100111001111","111100111101111",
        "111001001001001","111101111101111","111101111001111"};
    for(char c : text) {
        const char* glyph=c=='R' ? "110101110101101" : c=='C' ? "111100100100111" :
                          c=='-' ? "000000111000000" : digits[c-'0'];
        for(unsigned row=0;row<5;++row) for(unsigned col=0;col<3;++col)
            if(glyph[row*3+col]=='1') image.setPixel({x+col,y+row},sf::Color::White);
        x+=4;
    }
}

std::string padded(long long value,int width)
{
    std::ostringstream text;
    if(value<0) { text<<'-'; value=-value; }
    text<<std::setfill('0')<<std::setw(width)<<value;
    return text.str();
}

int main(int argc,char** argv) try
{
    if(argc<3) throw std::runtime_error("Usage: preview <region> [<x,y> ... | --cover] [--out dir] [--sheet] [--grid] [--aspect 4:3|16:9]");
    std::vector<sf::Vector2f> points;
    std::filesystem::path out="build/preview";
    sf::Vector2u physical{1920,1080};
    bool sheet=false,grid=false,cover=false;
    for(int i=2;i<argc;++i) {
        const std::string arg=argv[i];
        if(arg=="--cover") cover=true;
        else if(arg=="--sheet") sheet=true;
        else if(arg=="--grid") grid=true;
        else if(arg=="--out" || arg=="--aspect") {
            if(++i==argc) throw std::runtime_error("Missing value for "+arg);
            const std::string value=argv[i];
            if(arg=="--out") { if(value.empty() || value.starts_with("--")) throw std::runtime_error("Invalid output directory: "+value); out=value; }
            else if(value=="4:3") physical={1600,1200};
            else if(value=="16:9") physical={1920,1080};
            else throw std::runtime_error("Unsupported aspect: "+value);
        }
        else {
            if(arg.starts_with("--")) throw std::runtime_error("Unknown option: "+arg);
            points.push_back(coordinate(arg));
        }
    }
    if(cover && !points.empty()) throw std::runtime_error("--cover cannot be combined with explicit coordinates");
    if(!cover && points.empty()) throw std::runtime_error("At least one coordinate is required");
    const auto file=Region::findFile(argv[1]);
    const Region room(file);
    const Background background(file.parent_path().parent_path()/"backgrounds"/file.filename());
    sf::View view;
    Display::apply(view,physical);
    const sf::Vector2u size{static_cast<unsigned>(std::lround(view.getSize().x)),
                           static_cast<unsigned>(std::lround(view.getSize().y))};
    unsigned coverColumns=0,coverRows=0;
    if(cover) {
        const auto world=room.bounds();
        // Display's float arithmetic can leave an integer ratio a few ulps above its value.
        coverColumns=static_cast<unsigned>(std::ceil(world.size.x/view.getSize().x-0.00001f));
        coverRows=static_cast<unsigned>(std::ceil(world.size.y/view.getSize().y-0.00001f));
        for(unsigned row=0;row<coverRows;++row) for(unsigned col=0;col<coverColumns;++col)
            points.push_back(world.position+view.getSize()/2.f+
                sf::Vector2f{col*view.getSize().x,row*view.getSize().y});
        out/=argv[1];
        std::filesystem::create_directories(out/"cells");
        std::filesystem::create_directories(out/"rows");
    }
    sf::RenderTexture target(size);
    std::filesystem::create_directories(out);
    const unsigned columns=static_cast<unsigned>(std::min<std::size_t>(3,points.size()));
    const unsigned rows=static_cast<unsigned>((points.size()+columns-1)/columns);
    sf::Image contact;
    if(sheet && !cover) contact=sf::Image({columns*size.x,rows*(size.y+12)},sf::Color(25,30,45));
    sf::Image rowImage,overview;
    constexpr unsigned thumbWidth=200,gap=2,header=12;
    const unsigned thumbHeight=static_cast<unsigned>(std::lround(thumbWidth*view.getSize().y/view.getSize().x));
    nlohmann::json manifest={{"region",argv[1]}, {"viewSize",{view.getSize().x,view.getSize().y}},
        {"rows",coverRows},{"columns",coverColumns},{"cells",nlohmann::json::array()}};
    if(cover) overview=sf::Image({coverColumns*(thumbWidth+gap)+gap,
        coverRows*(thumbHeight+header+gap)+gap},sf::Color(25,30,45));
    for(std::size_t i=0;i<points.size();++i) {
        const auto half=view.getSize()/2.f;
        const auto world=room.bounds();
        const sf::Vector2f center{
            std::clamp(points[i].x,world.position.x+half.x,world.position.x+world.size.x-half.x),
            std::clamp(points[i].y,world.position.y+half.y,world.position.y+world.size.y-half.y)};
        view.setCenter(center);
        std::cout<<i+1<<": requested="<<points[i].x<<','<<points[i].y
                 <<" center="<<center.x<<','<<center.y
                 <<" bounds="<<center.x-half.x<<','<<center.y-half.y<<".."
                 <<center.x+half.x<<','<<center.y+half.y<<'\n';
        target.setView(view);
        target.clear(sf::Color::Black);
        sf::RectangleShape base(view.getSize());
        base.setPosition(center-half);
        base.setFillColor(sf::Color(25,30,45));
        target.draw(base);
        background.render(target);
        room.render(target);
        if(grid) diagnostic(target,view);
        target.display();
        auto image=target.getTexture().copyToImage();
        if(cover) {
            const unsigned row=static_cast<unsigned>(i/coverColumns),col=static_cast<unsigned>(i%coverColumns);
            const auto cell="R"+padded(row+1,2)+"-C"+padded(col+1,2);
            if(grid) {
                for(unsigned y=4;y<15;++y) for(unsigned x=4;x<42;++x) image.setPixel({x,y},sf::Color(25,30,45));
                label(image,7,7,cell);
            }
            const auto relative="cells/"+cell+"_x"+padded(std::llround(center.x),4)+
                "_y"+padded(std::llround(center.y),4)+".png";
            if(!image.saveToFile(out/relative)) throw std::runtime_error("Cell save failed");
            manifest["cells"].push_back({{"cell",cell},{"row",row+1},{"column",col+1},
                {"cameraCenter",{center.x,center.y}},
                {"visibleRect",{{"left",center.x-half.x},{"top",center.y-half.y},
                                {"width",view.getSize().x},{"height",view.getSize().y}}}, {"file",relative}});
            if(col==0) rowImage=sf::Image({coverColumns*size.x,size.y},sf::Color::Black);
            if(!rowImage.copy(image,{col*size.x,0})) throw std::runtime_error("Row copy failed");
            if(col+1==coverColumns && !rowImage.saveToFile(out/"rows"/("R"+padded(row+1,2)+".png")))
                throw std::runtime_error("Row save failed");
            const unsigned ox=gap+col*(thumbWidth+gap),oy=gap+row*(thumbHeight+header+gap);
            label(overview,ox+3,oy+3,cell);
            for(unsigned y=0;y<thumbHeight;++y) for(unsigned x=0;x<thumbWidth;++x)
                overview.setPixel({ox+x,oy+header+y},image.getPixel({x*size.x/thumbWidth,y*size.y/thumbHeight}));
            continue;
        }
        const auto output=out/("preview_"+std::to_string(i+1)+".png");
        if(!image.saveToFile(output)) throw std::runtime_error("Cannot save "+output.string());
        if(sheet) {
            const unsigned x=static_cast<unsigned>(i%columns)*size.x;
            const unsigned y=static_cast<unsigned>(i/columns)*(size.y+12);
            label(contact,x+4,y+3,std::to_string(i+1));
            if(!contact.copy(image,{x,y+12})) throw std::runtime_error("Sheet copy failed");
        }
    }
    if(cover) {
        if(!overview.saveToFile(out/"overview.png")) throw std::runtime_error("Overview save failed");
        std::ofstream output(out/"manifest.json");
        output<<manifest.dump(2)<<'\n';
        if(!output) throw std::runtime_error("Manifest save failed");
    }
    if(sheet && !cover && !contact.saveToFile(out/"contact_sheet.png")) throw std::runtime_error("Cannot save contact sheet");
}
catch(const std::exception& error)
{
    std::cerr<<"preview: "<<error.what()<<'\n';
    return 1;
}
