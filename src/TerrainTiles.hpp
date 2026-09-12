#pragma once
#include <SFML/Graphics.hpp>
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace Terrain {
inline constexpr std::array<const char*,10> roles{"inner","top","bottom","left","right",
    "top_left","top_right","bottom_left","bottom_right","slab"};
struct Tileset {
    std::filesystem::path image;
    int size;
    std::array<unsigned,10> variants;
    explicit Tileset(const std::filesystem::path& file);
};
struct Component { sf::FloatRect bounds; std::vector<sf::FloatRect> solids; };
struct Piece {
    sf::FloatRect bounds, uv;
    sf::Vector2f cell;
    unsigned role, variant;
};
std::uint64_t hashId(const std::string& id);
std::vector<Component> components(const std::vector<sf::FloatRect>& solids);
std::vector<Piece> pieces(const std::vector<Component>& groups,const Tileset& set,
                          std::uint64_t seed,const sf::FloatRect& visible);
}
class TerrainTiles {
public:
    TerrainTiles(const std::vector<sf::FloatRect>& solids,const std::filesystem::path& file,std::uint64_t seed);
    void render(sf::RenderTarget& target,const sf::FloatRect& visible) const;
private:
    Terrain::Tileset set_;
    std::vector<Terrain::Component> groups_;
    std::uint64_t seed_;
    mutable std::optional<sf::Texture> texture_;
};
