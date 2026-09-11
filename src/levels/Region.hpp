#pragma once
#include <SFML/Graphics/Rect.hpp>
#include <filesystem>
#include <vector>
namespace sf { class RenderTarget; }

// Region geometry only; player and camera lifetimes belong to the caller.
class Region
{
public:
    explicit Region(const std::filesystem::path& file);
    static std::filesystem::path findFile(const std::string& name,
        std::filesystem::path directory = std::filesystem::current_path());
    sf::FloatRect bounds() const { return bounds_; }
    sf::Vector2f spawn() const { return spawn_; }
    const std::vector<sf::FloatRect>& solids() const { return solids_; }
    void render(sf::RenderTarget& target) const;
private:
    sf::FloatRect bounds_;
    sf::Vector2f spawn_;
    std::vector<sf::FloatRect> solids_;
};
