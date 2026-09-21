#pragma once
#include <SFML/Graphics.hpp>
#include <deque>
#include <string>

class DebugHud
{
public:
    bool enabled=false;
    void update(double frameSeconds,sf::Vector2f feet);
    const std::string& performanceText() const { return performance_; }
    const std::string& worldText() const { return world_; }
    static bool hasGlyph(char c);
    sf::FloatRect panelBounds(sf::Vector2u pixels) const;
    void render(sf::RenderTarget& target) const;
private:
    struct Frame { double end,seconds; };
    std::deque<Frame> frames_;
    double time_=0,total_=0,lastRefresh_=0;
    std::string performance_="FPS 0.0  |  0.00 ms",world_="WORLD X 0.0  Y 0.0";
    unsigned scale(sf::Vector2u pixels) const;
};
