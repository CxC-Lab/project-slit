#include "Input.hpp"
#include "Player.hpp"

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <optional>
#include <vector>

int main()
{
    constexpr unsigned int windowWidth = 800;
    constexpr unsigned int windowHeight = 600;
    constexpr float worldWidth = 3200.f; // Current camera playtest size.
    constexpr float worldHeight = 600.f;
    constexpr float halfViewWidth = static_cast<float>(windowWidth) / 2.f;
    sf::RenderWindow window(sf::VideoMode({windowWidth, windowHeight}),
                            "Project Slit Prototype", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);
    sf::View camera(sf::FloatRect({0.f, 0.f},
                                  {static_cast<float>(windowWidth), static_cast<float>(windowHeight)}));

    const std::vector<sf::FloatRect> solids{
        {{0.f, 520.f}, {worldWidth, 80.f}}, // Floor across the entire room
        {{40.f, 180.f}, {24.f, 340.f}}, // Left wall
        {{220.f, 440.f}, {160.f, 20.f}},
        {{430.f, 360.f}, {140.f, 20.f}},
        {{950.f, 440.f}, {80.f, 80.f}},
        {{1350.f, 360.f}, {80.f, 160.f}},
        {{1800.f, 280.f}, {80.f, 240.f}},
        {{2300.f, 400.f}, {160.f, 20.f}},
        {{2850.f, 400.f}, {100.f, 120.f}}
    };
    sf::RectangleShape terrain;
    terrain.setFillColor(sf::Color(70, 80, 90));
    Player player;
    sf::RectangleShape playerShape(Movement::collisionSize);
    playerShape.setFillColor(sf::Color(240, 200, 100));
    Input input;
    sf::Clock frameClock;
    sf::Clock inputClock;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
            if (event->is<sf::Event::FocusLost>())
                input.reset();
            if (const auto* key = event->getIf<sf::Event::KeyReleased>())
                input.keyReleased(key->code);
            if (window.hasFocus())
                if (const auto* key = event->getIf<sf::Event::KeyPressed>())
                    input.keyPressed(key->code, inputClock.getElapsedTime().asSeconds());
        }
        if (!window.isOpen())
            break;

        // Discard long stalls; double-tap timing uses an independent, uncapped clock.
        const float deltaTime = std::min(frameClock.restart().asSeconds(), 0.05f);
        player.update(input.consume(), deltaTime, solids, worldWidth);
        const auto playerBounds = player.collisionBounds();
        playerShape.setPosition(playerBounds.position);
        // Follow horizontally; fixed Y keeps jumps from moving the screen vertically.
        const float centerX = playerBounds.position.x + playerBounds.size.x / 2.f;
        camera.setCenter({std::clamp(centerX, halfViewWidth, worldWidth - halfViewWidth),
                          worldHeight / 2.f});
        window.setView(camera);
        window.clear(sf::Color(25, 30, 45));
        for (const auto& solid : solids)
        {
            terrain.setPosition(solid.position);
            terrain.setSize(solid.size);
            window.draw(terrain);
        }
        window.draw(playerShape);
        window.display();
    }
}
