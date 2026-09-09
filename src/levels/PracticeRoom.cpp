#include "PracticeRoom.hpp"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

PracticeRoom::PracticeRoom()
{
    solids_ = {
        {{0.f, 520.f}, {bounds_.size.x, 80.f}}, // Floor across the entire room
        {{40.f, 180.f}, {24.f, 340.f}}, // Left wall
        {{220.f, 440.f}, {160.f, 20.f}},
        {{430.f, 360.f}, {140.f, 20.f}},
        {{950.f, 440.f}, {80.f, 80.f}},
        {{1350.f, 360.f}, {80.f, 160.f}},
        {{1800.f, 280.f}, {80.f, 240.f}},
        {{2300.f, 400.f}, {160.f, 20.f}},
        {{2850.f, 400.f}, {100.f, 120.f}},
        // Continue the existing wall upward for repeatable ninja-jump camera tests.
        {{40.f, -1050.f}, {24.f, 1230.f}},
        // Resting platforms beside the wall; the clear strip x=64..128 is climbable.
        {{128.f, 80.f}, {140.f, 20.f}},
        {{128.f, -160.f}, {140.f, 20.f}},
        {{128.f, -400.f}, {140.f, 20.f}},
        {{128.f, -640.f}, {140.f, 20.f}},
        {{128.f, -880.f}, {140.f, 20.f}},
        {{128.f, -1080.f}, {140.f, 20.f}}
    };
}

void PracticeRoom::render(sf::RenderTarget& target) const
{
    // This region currently draws the same rectangles used for collision.
    sf::RectangleShape terrain;
    terrain.setFillColor(sf::Color(70, 80, 90));
    for (const auto& solid : solids_)
    {
        terrain.setPosition(solid.position);
        terrain.setSize(solid.size);
        target.draw(terrain);
    }
}
