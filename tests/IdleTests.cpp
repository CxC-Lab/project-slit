#include "AnimationClip.hpp"

#include <SFML/Graphics/Image.hpp>
#include <iostream>
#include <stdexcept>

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main() try
{
    const auto manifest = AnimationClip::findManifest();
    const auto root = manifest.parent_path().parent_path().parent_path().parent_path();
    check(AnimationClip::findManifest(root) == manifest, "repo root path lookup");
    check(AnimationClip::findManifest(root / "build" / "Release") == manifest, "Release directory path lookup");
    AnimationClip idle(manifest);
    sf::Image image;
    check(image.loadFromFile(idle.atlasPath()), "atlas image loads without GL");
    idle.validateAtlas(image.getSize());
    check(idle.frames() == 4 && idle.fps() == 4. && idle.loops(), "current Idle manifest contract");
    check(idle.pivot() == sf::Vector2f(64.f, 123.f), "manifest pivot loaded");
    check(image.getSize() == sf::Vector2u(512, 128), "current atlas dimensions");
    for (int i = 0; i < idle.frames(); ++i)
    {
        check(idle.frameIndex() == i, "time advances frames left to right");
        const auto rect = idle.frameRect();
        check(rect.position.x == i * 128 && rect.position.y == 0 && rect.size == sf::Vector2i(128, 128), "frame slicing");
        check(rect.position.x + rect.size.x <= static_cast<int>(image.getSize().x) &&
              rect.position.y + rect.size.y <= static_cast<int>(image.getSize().y), "frame inside atlas");
        idle.advance(1. / idle.fps());
    }
    check(idle.frameIndex() == 0, "full cycle wraps to first frame");
    idle.advance(2. / idle.fps());
    check(idle.frameIndex() == 2, "one long update skips frames correctly");
    idle.advance(10. * idle.frames() / idle.fps());
    check(idle.frameIndex() == 2, "long delta wraps multiple cycles");
    AnimationClip smallSteps(manifest);
    for (int i = 0; i < 75; ++i) smallSteps.advance(0.01);
    check(smallSteps.frameIndex() == 3, "small deltas accumulate by elapsed time");
    bool rejected = false;
    try { idle.validateAtlas({511, 128}); } catch (const std::exception&) { rejected = true; }
    check(rejected, "atlas mismatch is rejected");
    AnimationClip walking(manifest, "Walking");
    sf::Image walkingImage;
    check(walkingImage.loadFromFile(walking.atlasPath()), "Walking atlas loads");
    walking.validateAtlas(walkingImage.getSize());
    check(walking.frames() == 4 && walking.fps() == 4. && walking.loops(), "Walking manifest contract");
    check(walking.pivot() == idle.pivot() && walking.frameRect().size == idle.frameRect().size, "shared canvas and pivot");
    for (int i = 0; i < walking.frames(); ++i)
    {
        const auto rect = walking.frameRect();
        check(walking.frameIndex() == i && rect.position.x == i * rect.size.x && rect.position.y == 0,
              "Walking left-to-right frame slicing");
        check(rect.position.x + rect.size.x <= static_cast<int>(walkingImage.getSize().x) &&
              rect.position.y + rect.size.y <= static_cast<int>(walkingImage.getSize().y), "Walking rect in atlas");
        walking.advance(1. / walking.fps());
    }
    check(walking.frameIndex() == 0, "Walking loop wrap");
    walking.advance(1.5 / walking.fps());
    walking.reset();
    check(walking.frameIndex() == 0, "clip change resets frame");
    walking.advance(0.75 / walking.fps());
    check(walking.frameIndex() == 0, "clip change resets fractional timer");
    std::cout << "PASS: manifest, atlas, paths, rectangles, timing and wrap\n";
}
catch (const std::exception& error)
{
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
}
