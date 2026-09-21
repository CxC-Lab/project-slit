#pragma once
#include "ConsoleCommands.hpp"
#include <SFML/Graphics.hpp>
#include <deque>
class DevConsole {
public:
    bool isOpen() const { return open_; }
    const std::string& inputLine() const { return input_; }
    const std::deque<std::string>& history() const { return history_; }
    bool handleEvent(const sf::Event& event,const RuntimeControls& controls);
    void render(sf::RenderTarget& target) const;
    static sf::FloatRect panelBounds(sf::Vector2u pixels);
    static sf::FloatRect inputBounds(sf::Vector2u pixels);
private:
    void record(const std::string& line);
    bool open_=false,suppressToggleText_=false;
    std::string input_;
    std::deque<std::string> history_;
};
