#pragma once

#include "Player.hpp"
#include <SFML/Window/Keyboard.hpp>
#include <array>

class Input
{
public:
    void keyPressed(sf::Keyboard::Key key, float time);
    void keyReleased(sf::Keyboard::Key key);
    void reset();
    InputIntent consume();

private:
    sf::Vector2f direction() const;
    std::array<bool, 8> held_{};
    std::array<float, 4> lastTap_{};
    std::array<bool, 4> tapPending_{};
    bool spaceHeld_ = false;
    bool jumpRequested_ = false;
    sf::Vector2f dashRequested_{};
};
