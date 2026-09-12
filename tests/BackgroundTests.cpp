#include "Background.hpp"
#include "AnimationClip.hpp"
#include <SFML/Graphics/Sprite.hpp>
#include "levels/Region.hpp"
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
void check(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
int main(int argc, char** argv) try {
    const auto region = Region::findFile("avatar_lake");
    const auto file = region.parent_path().parent_path()/"backgrounds"/region.filename();
    const auto layers = Background::readLayers(file);
    check(layers.size()==3,"avatar_lake data composition");
    check(Background::readLayers(file.parent_path()/"practice_room.json").empty(),"absent data uses flat background");
    for(std::size_t i=0;i<layers.size();++i) {
        const auto& layer=layers[i];
        sf::Image image;
        check(image.loadFromFile(layer.image),"image load");
        const auto size=image.getSize();
        check(size.y>=600+5400*layer.parallax.y,"full vertical travel plus maximum reference height");
        for(float height : {600.f,450.f}) {
            const float cmin=-4800+height/2, cmax=600-height/2;
            check(size.y+.01f>=height+(cmax-cmin)*layer.parallax.y,"derived height bound");
            if(i==0) check(Background::position(layer,{0,cmin}).y<=cmin-height/2+.01f,"sky top covers minimum camera");
            check(Background::position(layer,{0,cmax}).y+size.y>=cmax+height/2-.01f,"layer bottom covers maximum camera");
        }
        for(unsigned y=0;y<size.y;++y) {
            check(image.getPixel({0,y})==image.getPixel({size.x-1,y}),"tile edge continuity");
            for(unsigned x=0;x<size.x;++x) {
                const auto alpha=image.getPixel({x,y}).a;
                check(alpha==0 || alpha==255,"binary alpha across every pixel");
                if(i==0) check(alpha==255,"base layer fully opaque");
            }
        }
        for(float height : {450.f,600.f}) for(float x : {400.f,4800.f,9200.f})
            for(float y : {-4800.f+height/2.f,-2100.f,600.f-height/2.f}) {
                sf::View view(sf::FloatRect({0,0},{800,height})); view.setCenter({x,y});
                const auto visible=Background::visibleRect(layer,size,view);
                check(visible.has_value() && std::abs(visible->size.x-800)<.01f &&
                      std::abs(visible->size.y-height)<.01f,"no coverage gaps at vertical and horizontal extremes");
            }
    }
    const auto quad=Background::makeQuad({{-137.5f,-481.25f},{800,600}},{316.25f,-88.5f});
    for(std::size_t a=0;a<quad.getVertexCount();++a) for(std::size_t b=0;b<quad.getVertexCount();++b)
        check(quad[a].texCoords-quad[b].texCoords==quad[a].position-quad[b].position,"render quad keeps texel/world differences identical on both axes");
    auto layer=layers[0]; layer.parallax={1,1};
    check(Background::position(layer,{100,200})==layer.anchor,"parallax one world fixed");
    layer.parallax={0,0};
    check(Background::position(layer,{100,200})==layer.anchor+sf::Vector2f(100,200),"parallax zero screen fixed");
    layer.repeatX=false; layer.parallax={1,1}; layer.anchor={10000,10000};
    sf::View view(sf::FloatRect({0,0},{800,600}));
    check(!Background::visibleRect(layer,{100,100},view),"offscreen layer culled");
    const auto temporary=std::filesystem::temp_directory_path()/"slit_bad_background.json";
    { std::ofstream out(temporary); out << R"({"layers":[{"image":"bad.png"}]})"; }
    bool rejected=false;
    try { (void)Background::readLayers(temporary); } catch(const std::exception&) { rejected=true; }
    std::filesystem::remove(temporary);
    check(rejected,"missing fields fail clearly");
    if(argc>1 && std::string(argv[1])=="--gui") {
        Background background(file);
        Region room(region);
        AnimationClip idle(AnimationClip::findManifest());
        sf::Texture texture(idle.atlasPath());
        texture.setSmooth(false);
        sf::Sprite sprite(texture,idle.frameRect());
        sprite.setOrigin(idle.pivot());
        sprite.setScale({.5f,.5f});
        sprite.setPosition({116,520});
        for(unsigned height : {450u,600u}) {
            sf::RenderTexture target({800,height});
            int index=0;
            for(float y : {600.f-height/2.f,-2100.f,-4800.f+height/2.f}) {
                sf::View camera(sf::FloatRect({0,0},{800,static_cast<float>(height)}));
                camera.setCenter({400,y});
                target.setView(camera);
                const sf::Color clear(255,0,255,0);
                target.clear(clear);
                background.render(target);
                target.display();
                const auto rendered=target.getTexture().copyToImage();
                for(unsigned py=0;py<height;++py) for(unsigned px=0;px<800;++px)
                    check(rendered.getPixel({px,py}).a==255 && rendered.getPixel({px,py})!=clear,
                          "GPU no clear color exposed at either aspect, top/middle/bottom");
                room.render(target);
                target.draw(sprite);
                target.display();
                const char* names[]{"bg02_spawn.png","bg02_middle.png","bg02_top.png"};
                if(height==450) {
                    const auto output=region.parent_path().parent_path().parent_path()/"build"/names[index];
                    check(target.getTexture().copyToImage().saveToFile(output),"preview save");
                }
                ++index;
            }
        }
        std::cout<<"GPU: three textures, both aspects fully covered, spawn/middle/top screenshots saved\n";
    }
    std::cout<<"PASS: background paths, binary alpha, seams, coverage, placement and culling\n";
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
