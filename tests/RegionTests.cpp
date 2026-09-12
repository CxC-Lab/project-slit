#include "levels/Region.hpp"
#include "Player.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

// This 10-unit connectivity check detects sealed, player-sized empty regions only.
// It does not prove movement reachability, fun, composition, difficulty or route quality,
// and never replaces human playtesting. Exact collider checks erode free space;
// grid alignment can conservatively reject even a passage exactly as wide as the player.
namespace {
constexpr float step=10.f;
std::filesystem::path regionDirectory()
{
    auto directory=std::filesystem::current_path();
    while(true) {
        const auto candidate=directory/"assets"/"regions";
        if(std::filesystem::is_directory(candidate)) return candidate;
        const auto parent=directory.parent_path();
        if(parent==directory) throw std::runtime_error("Cannot find assets/regions");
        directory=parent;
    }
}

void inspect(const std::filesystem::path& file)
{
    const Region region(file);
    const auto bounds=region.bounds();
    const auto spawn=Player(region.spawn()).collisionBounds(); // Spawn is the collider's top-left.
    for(const auto& solid : region.solids())
        if(spawn.findIntersection(solid)) throw std::runtime_error(file.string()+": spawn collider overlaps terrain");
    const int columns=static_cast<int>(std::ceil(bounds.size.x/step));
    const int rows=static_cast<int>(std::ceil(bounds.size.y/step));
    const auto freeRectangle=[&](const sf::FloatRect& rectangle) {
        const auto end=rectangle.position+rectangle.size;
        const auto limit=bounds.position+bounds.size;
        if(rectangle.position.x<bounds.position.x || rectangle.position.y<bounds.position.y ||
           end.x>limit.x || end.y>limit.y) return false;
        for(const auto& solid : region.solids()) if(rectangle.findIntersection(solid)) return false;
        return true;
    };
    std::vector<int> cells(columns*rows,-1); // Candidate collider origins: -1 blocked, 0 unvisited.
    for(int y=0;y<rows;++y) for(int x=0;x<columns;++x)
        if(freeRectangle({bounds.position+sf::Vector2f{x*step,y*step},Movement::collisionSize}))
            cells[y*columns+x]=0;
    // Adjacent origins differ by 10, less than either collider dimension: the two
    // endpoint rectangles cover the entire axis-aligned sweep, so no wall is skipped.
    const auto flood=[&](int start,int id) {
        std::vector<int> queue{start};
        cells[start]=id;
        for(std::size_t cursor=0;cursor<queue.size();++cursor) {
            const int at=queue[cursor],x=at%columns,y=at/columns;
            const auto visit=[&](int nx,int ny) {
                if(nx<0 || nx>=columns || ny<0 || ny>=rows) return;
                const int next=ny*columns+nx;
                if(cells[next]==0) { cells[next]=id; queue.push_back(next); }
            };
            visit(x-1,y); visit(x+1,y); visit(x,y-1); visit(x,y+1);
        }
        return queue;
    };
    const auto offset=spawn.position-bounds.position;
    const int sx=static_cast<int>(std::floor(offset.x/step)),sy=static_cast<int>(std::floor(offset.y/step));
    bool seeded=false;
    for(int y=sy;y<=sy+1;++y) for(int x=sx;x<=sx+1;++x) {
        if(x<0 || x>=columns || y<0 || y>=rows || cells[y*columns+x]<0) continue;
        const auto position=bounds.position+sf::Vector2f{x*step,y*step};
        const sf::Vector2f lo{std::min(position.x,spawn.position.x),std::min(position.y,spawn.position.y)};
        const sf::Vector2f hi{std::max(position.x,spawn.position.x),std::max(position.y,spawn.position.y)};
        // Connect the exact, potentially non-grid-aligned spawn without crossing terrain.
        if(!freeRectangle({lo,hi-lo+Movement::collisionSize})) continue;
        seeded=true;
        if(cells[y*columns+x]==0) flood(y*columns+x,1);
    }
    if(!seeded) throw std::runtime_error(file.string()+": spawn has no reachable grid seed");
    for(int start=0;start<columns*rows;++start) {
        if(cells[start]!=0) continue;
        const auto component=flood(start,2);
        int minX=columns,minY=rows,maxX=0,maxY=0;
        for(int at : component) {
            const int x=at%columns,y=at/columns;
            minX=std::min(minX,x); minY=std::min(minY,y);
            maxX=std::max(maxX,x); maxY=std::max(maxY,y);
        }
        std::ostringstream error;
        error<<file.filename().string()<<": isolated player-sized region x=["
             <<bounds.position.x+minX*step<<','<<bounds.position.x+maxX*step+Movement::collisionSize.x
             <<"] y=["<<bounds.position.y+minY*step<<','
             <<bounds.position.y+maxY*step+Movement::collisionSize.y<<']';
        throw std::runtime_error(error.str());
    }
    std::cout<<"PASS: "<<file.filename().string()<<'\n';
}
}
int main(int argc,char** argv) try
{
    const auto start=std::chrono::steady_clock::now();
    if(argc==3 && std::string(argv[1])=="--file") inspect(argv[2]); // Temporary negative fixtures, outside assets.
    else if(argc==1) {
        std::vector<std::filesystem::path> files;
        for(const auto& entry : std::filesystem::directory_iterator(regionDirectory()))
            if(entry.is_regular_file() && entry.path().extension()==".json") files.push_back(entry.path());
        if(files.empty()) throw std::runtime_error("No region JSON files found");
        std::sort(files.begin(),files.end());
        for(const auto& file : files) inspect(Region::findFile(file.stem().string(),file.parent_path()));
    } else throw std::runtime_error("Usage: region_tests [--file temporary-region.json]");
    std::cout<<"Elapsed: "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"s\n";
}
catch(const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n'; return 1; }
