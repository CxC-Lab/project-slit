#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>

class Display
{
public:
    // Replace the reference composition/aspect policy here, independently of physical pixels.
    inline static constexpr sf::Vector2f referenceViewSize{800.f, 600.f};
    static sf::FloatRect viewport(sf::Vector2u physicalSize);
    static void apply(sf::View& view, sf::Vector2u physicalSize);
    void open(sf::RenderWindow& window);
    bool handleEvent(const sf::Event& event, sf::RenderWindow& window, sf::View& worldView);
    bool fullscreen() const { return fullscreen_; }

private:
    void toggle(sf::RenderWindow& window, sf::View& worldView);
    bool fullscreen_ = false;
    sf::Vector2u windowedSize_{static_cast<unsigned int>(referenceViewSize.x),
                               static_cast<unsigned int>(referenceViewSize.y)};
    sf::Vector2i windowedPosition_{};
};
