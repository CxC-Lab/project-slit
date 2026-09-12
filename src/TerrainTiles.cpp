#include "TerrainTiles.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

Terrain::Tileset::Tileset(const std::filesystem::path& file)
{
    std::ifstream input(file);
    if(!input) throw std::runtime_error("Cannot open tileset: "+file.string());
    const auto data=nlohmann::json::parse(input);
    if(!data.at("tileSize").is_number_integer()) throw std::runtime_error("tileSize must be integer");
    size=data.at("tileSize").get<int>();
    if(size<=0) throw std::runtime_error("tileSize must be positive");
    image=file.parent_path()/data.at("image").get<std::string>();
    for(unsigned i=0;i<roles.size();++i) {
        const auto& n=data.at("roles").at(roles[i]);
        if(!n.is_number_integer() || n.get<std::int64_t>()<=0) throw std::runtime_error("Invalid tile variant count");
        variants[i]=n.get<unsigned>();
    }
}

std::uint64_t Terrain::hashId(const std::string& id)
{
    std::uint64_t h=14695981039346656037ull;
    for(unsigned char c : id) { h^=c; h*=1099511628211ull; }
    return h;
}
namespace {
std::uint64_t tileHash(sf::Vector2f cell,std::uint64_t seed)
{
    std::uint64_t h=14695981039346656037ull;
    // FNV-1a, explicitly little-endian uint64 words, including signed coordinate bits.
    for(std::uint64_t word : {seed,static_cast<std::uint64_t>(static_cast<std::int64_t>(std::floor(cell.x))),
                             static_cast<std::uint64_t>(static_cast<std::int64_t>(std::floor(cell.y)))})
        for(int i=0;i<8;++i) { h^=(word>>(i*8))&255; h*=1099511628211ull; }
    return h;
}
bool connected(const sf::FloatRect& a,const sf::FloatRect& b)
{
    const auto ae=a.position+a.size,be=b.position+b.size;
    const float x=std::min(ae.x,be.x)-std::max(a.position.x,b.position.x);
    const float y=std::min(ae.y,be.y)-std::max(a.position.y,b.position.y);
    return (x>0 && y>=0) || (x>=0 && y>0); // Exclude point-only contact.
}
void sortCuts(std::vector<float>& cuts)
{
    std::sort(cuts.begin(),cuts.end());
    cuts.erase(std::unique(cuts.begin(),cuts.end()),cuts.end());
}
// Exact union cross-section through an atomic rectangle's interior.
sf::Vector2f span(const Terrain::Component& group,sf::Vector2f p,bool horizontal)
{
    std::vector<sf::Vector2f> intervals;
    for(const auto& r:group.solids) {
        const auto end=r.position+r.size;
        if(horizontal ? (p.y>r.position.y && p.y<end.y) : (p.x>r.position.x && p.x<end.x))
            intervals.push_back(horizontal ? sf::Vector2f{r.position.x,end.x} : sf::Vector2f{r.position.y,end.y});
    }
    std::sort(intervals.begin(),intervals.end(),[](auto a,auto b){return a.x<b.x;});
    const float at=horizontal?p.x:p.y;
    sf::Vector2f range=intervals.front();
    for(std::size_t i=1;i<intervals.size();++i) {
        if(intervals[i].x<=range.y) range.y=std::max(range.y,intervals[i].y);
        else { if(at>=range.x && at<=range.y) return range; range=intervals[i]; }
    }
    return range;
}
}
std::vector<Terrain::Component> Terrain::components(const std::vector<sf::FloatRect>& solids)
{
    std::vector<bool> used(solids.size());
    std::vector<Component> result;
    for(std::size_t start=0;start<solids.size();++start) {
        if(used[start]) continue;
        used[start]=true;
        std::vector<std::size_t> queue{start};
        Component group{solids[start],{}};
        for(std::size_t i=0;i<queue.size();++i) {
            const auto r=solids[queue[i]];
            group.solids.push_back(r);
            const auto end=group.bounds.position+group.bounds.size,re=r.position+r.size;
            const sf::Vector2f lo{std::min(group.bounds.position.x,r.position.x),std::min(group.bounds.position.y,r.position.y)};
            group.bounds={lo,{std::max(end.x,re.x)-lo.x,std::max(end.y,re.y)-lo.y}};
            for(std::size_t j=0;j<solids.size();++j)
                if(!used[j] && connected(r,solids[j])) { used[j]=true; queue.push_back(j); }
        }
        result.push_back(std::move(group));
    }
    return result;
}
std::vector<Terrain::Piece> Terrain::pieces(const std::vector<Component>& groups,const Tileset& set,
                                           std::uint64_t seed,const sf::FloatRect& visible)
{
    std::vector<Piece> output;
    const float t=static_cast<float>(set.size);
    for(const auto& group:groups) {
        const auto clipped=group.bounds.findIntersection(visible);
        if(!clipped) continue;
        const auto origin=group.bounds.position;
        const auto first=clipped->position-origin,last=first+clipped->size;
        for(int row=static_cast<int>(std::floor(first.y/t));row<static_cast<int>(std::ceil(last.y/t));++row)
        for(int col=static_cast<int>(std::floor(first.x/t));col<static_cast<int>(std::ceil(last.x/t));++col) {
            const sf::Vector2f cell=origin+sf::Vector2f{col*t,row*t};
            std::vector<float> xs{cell.x,cell.x+t},ys{cell.y,cell.y+t};
            // Solid edges partition this cell into disjoint constant-occupancy rectangles.
            for(const auto& r:group.solids) {
                for(float x:{r.position.x,r.position.x+r.size.x}) if(x>cell.x && x<cell.x+t) xs.push_back(x);
                for(float y:{r.position.y,r.position.y+r.size.y}) if(y>cell.y && y<cell.y+t) ys.push_back(y);
            }
            sortCuts(xs); sortCuts(ys);
            for(std::size_t y=1;y<ys.size();++y) for(std::size_t x=1;x<xs.size();++x) {
                const sf::FloatRect atom{{xs[x-1],ys[y-1]},{xs[x]-xs[x-1],ys[y]-ys[y-1]}};
                const auto mid=atom.position+atom.size/2.f;
                if(std::none_of(group.solids.begin(),group.solids.end(),[&](const auto& r){return r.contains(mid);})) continue;
                const auto h=span(group,mid,true),v=span(group,mid,false);
                std::vector<float> ax{atom.position.x,atom.position.x+atom.size.x},ay{atom.position.y,atom.position.y+atom.size.y};
                // Narrow strips meet at their midpoint; never stretch either edge's artwork.
                const float mx=(h.x+h.y)/2.f,my=(v.x+v.y)/2.f;
                if(h.y-h.x<2*t && mx>ax.front() && mx<ax.back()) ax.insert(ax.begin()+1,mx);
                if(v.y-v.x<2*t && my>ay.front() && my<ay.back()) ay.insert(ay.begin()+1,my);
                for(std::size_t iy=1;iy<ay.size();++iy) for(std::size_t ix=1;ix<ax.size();++ix) {
                    sf::FloatRect piece{{ax[ix-1],ay[iy-1]},{ax[ix]-ax[ix-1],ay[iy]-ay[iy-1]}};
                    const auto m=piece.position+piece.size/2.f;
                    const bool narrowX=h.y-h.x<2*t,narrowY=v.y-v.x<2*t;
                    const bool left=narrowX ? m.x<mx : cell.x<=h.x;
                    const bool right=narrowX ? m.x>=mx : cell.x+t>=h.y;
                    const bool top=narrowY ? m.y<my : cell.y<=v.x;
                    const bool bottom=narrowY ? m.y>=my : cell.y+t>=v.y;
                    unsigned role=0;
                    // Opposite vertical exposure -> slab. Other unsupported combinations
                    // prefer vertical edges, then left, then right (corners keep both axes).
                    if(v.y-v.x<=t) role=9;
                    else if(top) role=left?5:right?6:1;
                    else if(bottom) role=left?7:right?8:2;
                    else if(left) role=3;
                    else if(right) role=4;
                    const unsigned variant=static_cast<unsigned>(tileHash(cell,seed)%set.variants[role]);
                    sf::Vector2f uv=piece.position-cell;
                    if(left) uv.x=piece.position.x-h.x;
                    else if(right) uv.x=t-(h.y-piece.position.x);
                    if(top) uv.y=piece.position.y-v.x;
                    else if(bottom) uv.y=t-(v.y-piece.position.y);
                    const auto draw=piece.findIntersection(visible);
                    if(!draw) continue;
                    uv+=draw->position-piece.position+sf::Vector2f{variant*t,role*t};
                    output.push_back({*draw,{uv,draw->size},cell,role,variant});
                }
            }
        }
    }
    return output;
}
TerrainTiles::TerrainTiles(const std::vector<sf::FloatRect>& solids,const std::filesystem::path& file,std::uint64_t seed)
    :set_(file),groups_(Terrain::components(solids)),seed_(seed) {}
void TerrainTiles::render(sf::RenderTarget& target,const sf::FloatRect& visible) const
{
    if(!texture_) {
        texture_.emplace();
        if(!texture_->loadFromFile(set_.image)) { texture_.reset(); throw std::runtime_error("Cannot load tileset image"); }
        const auto size=texture_->getSize();
        if(size.x!=*std::max_element(set_.variants.begin(),set_.variants.end())*set_.size || size.y!=Terrain::roles.size()*set_.size)
            throw std::runtime_error("Tileset image dimensions mismatch");
        texture_->setSmooth(false);
    }
    sf::VertexArray vertices(sf::PrimitiveType::Triangles);
    for(const auto& p:Terrain::pieces(groups_,set_,seed_,visible)) {
        const auto a=p.bounds.position,b=a+p.bounds.size;
        for(sf::Vector2f point:{a,sf::Vector2f{b.x,a.y},b,a,b,sf::Vector2f{a.x,b.y}})
            vertices.append(sf::Vertex{point,sf::Color::White,p.uv.position+point-a});
    }
    sf::RenderStates states; states.texture=&*texture_;
    target.draw(vertices,states);
}
