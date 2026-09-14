#pragma once
#include <SFML/Graphics.hpp>
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace Terrain {
enum class Sampling { Atlas, Repeat2D, Edge, Corner };
enum Exposure : unsigned { N=1, S=2, W=4, E=8 };
inline constexpr std::array<const char*,10> roles{"inner","top","bottom","left","right",
    "top_left","top_right","bottom_left","bottom_right","slab"};
inline constexpr std::array<unsigned,10> exposures{0,N,S,W,E,N|W,N|E,S|W,S|E,N|S};
struct RoleSampling { Sampling mode=Sampling::Atlas; std::filesystem::path image; bool flip=false; };
struct Tileset {
    std::filesystem::path image;
    int size;
    std::array<unsigned,10> variants;
    std::array<RoleSampling,10> sampling{};
    explicit Tileset(const std::filesystem::path& file);
};
struct Component { sf::FloatRect bounds; std::vector<sf::FloatRect> solids; };
struct Piece {
    sf::FloatRect bounds, uv;
    sf::Vector2f cell;
    unsigned role, variant;
    Sampling sampling = Sampling::Atlas;
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
    const auto& sampling() const { return set_.sampling; }
private:
    Terrain::Tileset set_;
    std::vector<Terrain::Component> groups_;
    std::uint64_t seed_;
    mutable std::optional<sf::Texture> texture_;
    struct ImageTexture { std::filesystem::path path; sf::Texture texture; };
    mutable std::vector<ImageTexture> images_;
};
