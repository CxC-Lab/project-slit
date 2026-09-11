#include "Display.hpp"

#include <algorithm>
#include <iostream>

sf::FloatRect Display::viewport(sf::Vector2u)
{
    // Cover policy always renders to the entire physical window.
    return {{0.f, 0.f}, {1.f, 1.f}};
}

void Display::apply(sf::View& view, sf::Vector2u physicalSize)
{
    // Uniform Scale to Fill: crop the reference composition, never stretch it.
    sf::Vector2f visibleWorld = referenceViewSize;
    if (physicalSize.x != 0 && physicalSize.y != 0)
    {
        const float scale = std::max(physicalSize.x / referenceViewSize.x,
                                     physicalSize.y / referenceViewSize.y);
        visibleWorld = {physicalSize.x / scale, physicalSize.y / scale};
    }
    // Preserve the gameplay camera center, including zero-size/minimized events.
    view.setSize(visibleWorld);
    view.setViewport(viewport(physicalSize));
}

void Display::open(sf::RenderWindow& window)
{
    const auto desktop = sf::VideoMode::getDesktopMode();
    windowedPosition_ = {static_cast<int>((desktop.size.x - std::min(desktop.size.x, windowedSize_.x)) / 2),
                         static_cast<int>((desktop.size.y - std::min(desktop.size.y, windowedSize_.y)) / 2)};
    window.create(desktop, "Project Slit Prototype", sf::Style::None, sf::State::Windowed);
    window.setPosition({0, 0});
    fullscreen_ = true;
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);
    window.setMouseCursorVisible(!window.hasFocus());
}

void Display::toggle(sf::RenderWindow& window, sf::View& worldView)
{
    if (!fullscreen_)
    {
        windowedSize_ = window.getSize();
        windowedPosition_ = window.getPosition();
        // Desktop mode, no exclusive video-mode switch. SFML desktop mode is the primary display.
        const auto desktop = sf::VideoMode::getDesktopMode();
        window.create(desktop, "Project Slit Prototype", sf::Style::None, sf::State::Windowed);
        window.setPosition({0, 0});
    }
    else
    {
        window.create(sf::VideoMode(windowedSize_), "Project Slit Prototype", sf::Style::Default);
        window.setPosition(windowedPosition_);
    }
    fullscreen_ = !fullscreen_;
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);
    window.setMouseCursorVisible(!window.hasFocus());
    apply(worldView, window.getSize());
    window.setView(worldView);
    window.requestFocus();
    std::cout << "Display: " << (fullscreen_ ? "borderless" : "windowed")
              << ' ' << window.getSize().x << 'x' << window.getSize().y << std::endl;
}

bool Display::handleEvent(const sf::Event& event, sf::RenderWindow& window, sf::View& worldView)
{
    if (event.is<sf::Event::FocusLost>()) window.setMouseCursorVisible(true);
    if (event.is<sf::Event::FocusGained>()) window.setMouseCursorVisible(false);
    if (event.is<sf::Event::Closed>()) window.setMouseCursorVisible(true);
    if (const auto* key = event.getIf<sf::Event::KeyReleased>(); key && key->code == sf::Keyboard::Key::Enter)
    {
        return true;
    }
    if (const auto* key = event.getIf<sf::Event::KeyPressed>(); key && key->code == sf::Keyboard::Key::Enter)
    {
        if (key->alt)
            toggle(window, worldView);
        return true;
    }
    if (const auto* resized = event.getIf<sf::Event::Resized>())
    {
        apply(worldView, resized->size);
        return true;
    }
    return false;
}
