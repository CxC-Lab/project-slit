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
    sf::RenderWindow window(sf::VideoMode({windowWidth, windowHeight}),
                            "Project Slit Prototype", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    const std::vector<sf::FloatRect> solids{
        {{0.f, 520.f}, {800.f, 80.f}},   // Floor
        {{40.f, 180.f}, {24.f, 340.f}}, // Left wall
        {{220.f, 440.f}, {160.f, 20.f}},
        {{430.f, 360.f}, {140.f, 20.f}}
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
        player.update(input.consume(), deltaTime, solids, static_cast<float>(windowWidth));
        playerShape.setPosition(player.collisionBounds().position);
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
