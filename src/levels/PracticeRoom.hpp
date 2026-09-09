#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <vector>

namespace sf { class RenderTarget; }

// Permanent practice region. Player, input and camera lifetimes belong to the caller.
class PracticeRoom
{
public:
    PracticeRoom();
    sf::FloatRect bounds() const { return bounds_; }
    sf::Vector2f spawn() const { return spawn_; }
    const std::vector<sf::FloatRect>& solids() const { return solids_; }
    void render(sf::RenderTarget& target) const;

private:
    sf::FloatRect bounds_{{0.f, -1200.f}, {3200.f, 1800.f}};
    sf::Vector2f spawn_{100.f, 472.f};
    std::vector<sf::FloatRect> solids_;
};
