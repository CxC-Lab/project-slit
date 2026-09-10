#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <filesystem>

// Only the proto_one Idle clip; no movement-state mapping or resource ownership.
class IdleAnimation
{
public:
    explicit IdleAnimation(const std::filesystem::path& manifest);
    static std::filesystem::path findManifest(std::filesystem::path directory = std::filesystem::current_path());
    void validateAtlas(sf::Vector2u size) const;
    void advance(double deltaTime);
    int frameIndex() const;
    sf::IntRect frameRect() const;
    sf::Vector2f pivot() const { return pivot_; }
    const std::filesystem::path& atlasPath() const { return atlasPath_; }
    int frames() const { return frames_; }
    double fps() const { return fps_; }
    bool loops() const { return loop_; }

private:
    sf::Vector2i canvas_{};
    sf::Vector2f pivot_{};
    std::filesystem::path atlasPath_;
    int row_ = 0;
    int frames_ = 0;
    double fps_ = 0.;
    bool loop_ = false;
    double elapsed_ = 0.;
};
