#include "Decorations.hpp"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

Decorations::Decorations(const std::filesystem::path& requested)
{
    file_=requested;
    if(requested.is_relative()) {
        for(auto directory=std::filesystem::current_path();;directory=directory.parent_path()) {
            if(std::filesystem::is_regular_file(directory/requested)){file_=directory/requested;break;}
            if(directory==directory.root_path())break;
        }
    }
    file_=std::filesystem::weakly_canonical(file_);
    std::ifstream input(file_,std::ios::binary);
    if(!input)throw std::runtime_error("Cannot open decorations: "+file_.string());
    const std::string bytes((std::istreambuf_iterator<char>(input)),{});
    std::uint64_t hash=14695981039346656037ull;
    for(unsigned char byte:bytes){hash^=byte;hash*=1099511628211ull;}
    std::ostringstream text;text<<std::hex<<std::setfill('0')<<std::setw(16)<<hash;hash_=text.str();
    const auto data=nlohmann::json::parse(bytes);
    const auto& entries=data.at("items");
    if(!entries.is_array())throw std::runtime_error("Decoration items must be an array");
    std::vector<std::filesystem::path> paths;
    for(const auto& entry:entries) {
        const std::filesystem::path relative(entry.at("image").get<std::string>());
        if(relative.empty()||relative.is_absolute())throw std::runtime_error("Decoration image must be relative");
        const auto path=std::filesystem::weakly_canonical(file_.parent_path()/relative);
        const auto& position=entry.at("position");
        if(!position.is_array()||position.size()!=2||!position[0].is_number()||!position[1].is_number())throw std::runtime_error("Invalid decoration position");
        const sf::Vector2f anchor{position[0].get<float>(),position[1].get<float>()};
        if(!std::isfinite(anchor.x)||!std::isfinite(anchor.y))throw std::runtime_error("Nonfinite decoration position");
        const auto layer=entry.at("layer").get<std::string>();
        if(layer!="behind"&&layer!="above")throw std::runtime_error("Invalid decoration layer");
        const bool flip=entry.at("flipX").get<bool>();
        auto found=std::find(paths.begin(),paths.end(),path);
        const auto index=static_cast<std::size_t>(found-paths.begin());
        if(found==paths.end()) {
            sf::Texture texture;
            if(!texture.loadFromFile(path))throw std::runtime_error("Cannot load decoration: "+path.string());
            texture.setSmooth(false);paths.push_back(path);textures_.push_back(std::move(texture));
        }
        items_.push_back({index,anchor,layer=="behind"?Layer::Behind:Layer::Above,flip});
    }
}

std::size_t Decorations::render(sf::RenderTarget& target,Layer layer) const
{
    const auto& view=target.getView();const sf::FloatRect screen{view.getCenter()-view.getSize()/2.f,view.getSize()};
    sf::VertexArray batch(sf::PrimitiveType::Triangles);std::size_t texture=0,count=0;
    const auto flush=[&]{if(batch.getVertexCount()){sf::RenderStates state;state.texture=&textures_[texture];target.draw(batch,state);batch.clear();}};
    for(const auto& item:items_) {
        if(item.layer!=layer)continue;
        const sf::Vector2f size(textures_[item.texture].getSize());
        const auto origin=item.position-sf::Vector2f{size.x/2.f,size.y};
        if(!screen.findIntersection({origin,size}))continue;
        // Only consecutive equal textures batch: A/B/A retains file-order occlusion.
        if(batch.getVertexCount()&&texture!=item.texture)flush();texture=item.texture;
        for(const sf::Vector2f uv:{sf::Vector2f{0,0},{size.x,0},size,{0,0},size,{0,size.y}})
            batch.append(sf::Vertex{origin+uv,sf::Color::White,{item.flip?size.x-uv.x:uv.x,uv.y}});
        ++count;
    }
    flush();return count;
}

nlohmann::json Decorations::captureIdentity() const
{
    return {{"decorations",{{"file",file_.generic_string()},{"hash",hash_},{"hashAlgorithm","fnv1a64"}}}};
}

DecorationSelection selectDecorations(const std::optional<std::filesystem::path>& regionDefault,
                                     const std::string& option,bool tilesetOverride)
{
    if(option=="none")return {{},"disabled"};
    if(!option.empty())return {std::filesystem::path(option),"override"};
    if(!tilesetOverride&&regionDefault)return {regionDefault,"region_default"};
    return {{},"disabled"};
}
