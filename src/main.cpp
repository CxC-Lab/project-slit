#include "Camera.hpp"
#include "Display.hpp"
#include "Input.hpp"
#include "AnimationClip.hpp"
#include "Player.hpp"
#include "levels/PracticeRoom.hpp"

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <optional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
struct ClipVisual
{
    AnimationClip clip;
    sf::Texture texture;
    unsigned int frameChanges = 0;

    ClipVisual(const std::filesystem::path& manifest, const std::string& name) : clip(manifest, name)
    {
        if (!texture.loadFromFile(clip.atlasPath()))
            throw std::runtime_error("Cannot load animation texture: " + clip.atlasPath().string());
        clip.validateAtlas(texture.getSize());
        texture.setSmooth(false);
    }
};
}

int main() try
{
    Display display;
    sf::RenderWindow window;
    display.open(window);
    sf::View camera(sf::FloatRect({0.f, 0.f}, Display::referenceViewSize));
    Display::apply(camera, window.getSize());

    const PracticeRoom room;
    Player player(room.spawn());
    sf::RectangleShape playerShape(Movement::collisionSize);
    playerShape.setFillColor(sf::Color(240, 200, 100));
    constexpr bool showPlayerCollider = false;
    constexpr float visualScale = 0.5f; // Rendering-only playtest candidate.
    const auto manifest = AnimationClip::findManifest();
    ClipVisual idle(manifest, "Idle");
    ClipVisual walking(manifest, "Walking");
    ClipVisual sprinting(manifest, "Sprinting");
    for (const auto* visual : {&walking, &sprinting})
        if (idle.clip.pivot() != visual->clip.pivot() || idle.clip.frameRect().size != visual->clip.frameRect().size)
            throw std::runtime_error("Idle, Walking and Sprinting must share canvas and pivot");
    ClipVisual* activeVisual = &idle;
    sf::Sprite playerSprite(idle.texture, idle.clip.frameRect());
    playerSprite.setOrigin(idle.clip.pivot());
    playerSprite.setScale({visualScale, visualScale});
    Input input;
    sf::Clock frameClock;
    sf::Clock inputClock;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
                break;
            }
            if (display.handleEvent(*event, window, camera))
            {
                if (event->is<sf::Event::KeyPressed>())
                    input.reset();
                continue;
            }
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
        ClipVisual* nextVisual = &idle;
        switch (player.state())
        {
        case MovementState::Walking: nextVisual = &walking; break;
        case MovementState::Sprinting: nextVisual = &sprinting; break;
        default: break;
        }
        const bool changedClip = nextVisual != activeVisual;
        if (changedClip)
        {
            activeVisual = nextVisual;
            activeVisual->clip.reset();
            playerSprite.setTexture(activeVisual->texture);
        }
        const int previousFrame = activeVisual->clip.frameIndex();
        if (!changedClip)
            activeVisual->clip.advance(deltaTime);
        activeVisual->frameChanges += activeVisual->clip.frameIndex() != previousFrame;
        playerSprite.setTextureRect(activeVisual->clip.frameRect());
        playerSprite.setPosition({playerBounds.position.x + playerBounds.size.x / 2.f,
                                  playerBounds.position.y + playerBounds.size.y});
        const bool catchUp = player.state() == MovementState::FastFalling ||
                             player.velocity().y >= Camera::catchUpFallSpeed;
        Camera::follow(camera, playerBounds.position + playerBounds.size / 2.f, room.bounds(), deltaTime,
                       catchUp ? Camera::catchUpMultiplier : 1.f);
        window.setTitle(std::string("Project Slit Prototype - ") + toString(player.state()));
        window.setView(camera);
        window.clear(sf::Color::Black);
        sf::RectangleShape viewBackground(camera.getSize());
        viewBackground.setPosition(camera.getCenter() - camera.getSize() / 2.f);
        viewBackground.setFillColor(sf::Color(25, 30, 45));
        window.draw(viewBackground);
        room.render(window);
        window.draw(playerSprite);
        if (showPlayerCollider)
            window.draw(playerShape);
        window.display();
    }
    std::cout << "Animation frame changes: Idle=" << idle.frameChanges
              << " Walking=" << walking.frameChanges << " Sprinting=" << sprinting.frameChanges << std::endl;
}
catch (const std::exception& error)
{
    std::cerr << "Project Slit startup/runtime error: " << error.what() << std::endl;
    return 1;
}
