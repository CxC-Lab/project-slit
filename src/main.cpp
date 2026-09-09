#include "Camera.hpp"
#include "Input.hpp"
#include "Player.hpp"
#include "levels/PracticeRoom.hpp"

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <optional>
#include <string>

int main()
{
    constexpr unsigned int windowWidth = 800;
    constexpr unsigned int windowHeight = 600;
    sf::RenderWindow window(sf::VideoMode({windowWidth, windowHeight}),
                            "Project Slit Prototype", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);
    sf::View camera(sf::FloatRect({0.f, 0.f},
                                  {static_cast<float>(windowWidth), static_cast<float>(windowHeight)}));

    const PracticeRoom room;
    Player player(room.spawn());
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
        player.update(input.consume(), deltaTime, room.solids(), room.bounds().size.x);
        const auto playerBounds = player.collisionBounds();
        playerShape.setPosition(playerBounds.position);
        const bool catchUp = player.state() == MovementState::FastFalling ||
                             player.velocity().y >= Camera::catchUpFallSpeed;
        Camera::follow(camera, playerBounds.position + playerBounds.size / 2.f, room.bounds(), deltaTime,
                       catchUp ? Camera::catchUpMultiplier : 1.f);
        window.setTitle(std::string("Project Slit Prototype - ") + toString(player.state()));
        window.setView(camera);
        window.clear(sf::Color(25, 30, 45));
        room.render(window);
        window.draw(playerShape);
        window.display();
    }
}
