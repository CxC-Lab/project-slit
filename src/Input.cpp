#include "Input.hpp"

namespace
{
int keyIndex(sf::Keyboard::Key key)
{
    using Key = sf::Keyboard::Key;
    switch (key)
    {
    case Key::A: return 0;
    case Key::Left: return 1;
    case Key::D: return 2;
    case Key::Right: return 3;
    case Key::W: return 4;
    case Key::Up: return 5;
    case Key::S: return 6;
    case Key::Down: return 7;
    default: return -1;
    }
}
}

sf::Vector2f Input::direction() const
{
    return {static_cast<float>(held_[2] || held_[3]) - static_cast<float>(held_[0] || held_[1]),
            static_cast<float>(held_[6] || held_[7]) - static_cast<float>(held_[4] || held_[5])};
}

void Input::keyPressed(sf::Keyboard::Key key, float time)
{
    if (key == sf::Keyboard::Key::Space)
    {
        jumpRequested_ |= !spaceHeld_;
        spaceHeld_ = true;
        return;
    }
    const int index = keyIndex(key);
    if (index < 0 || held_[index])
        return;
    const int axisDirection = index / 2;
    const bool alreadyHeld = held_[axisDirection * 2] || held_[axisDirection * 2 + 1];
    held_[index] = true;
    // A and Left (etc.) are aliases, not two taps when held together.
    if (alreadyHeld)
        return;
    if (tapPending_[axisDirection] && time - lastTap_[axisDirection] <= Movement::doubleTapWindow)
    {
        dashRequested_ = direction();
        tapPending_[axisDirection] = false;
    }
    else
    {
        tapPending_[axisDirection] = true;
        lastTap_[axisDirection] = time;
    }
}

void Input::keyReleased(sf::Keyboard::Key key)
{
    if (key == sf::Keyboard::Key::Space)
        spaceHeld_ = false;
    const int index = keyIndex(key);
    if (index >= 0)
        held_[index] = false;
}

void Input::reset()
{
    *this = Input{};
}

InputIntent Input::consume()
{
    const InputIntent intent{direction(), jumpRequested_, dashRequested_};
    jumpRequested_ = false;
    dashRequested_ = {};
    return intent;
}
