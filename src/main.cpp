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
    AnimationClip idle(manifest);
    AnimationClip walking(manifest, "Walking");
    sf::Texture idleTexture;
    if (!idleTexture.loadFromFile(idle.atlasPath()))
        throw std::runtime_error("Cannot load Idle texture: " + idle.atlasPath().string());
    idle.validateAtlas(idleTexture.getSize());
    idleTexture.setSmooth(false);
    sf::Texture walkingTexture;
    if (!walkingTexture.loadFromFile(walking.atlasPath()))
        throw std::runtime_error("Cannot load Walking texture: " + walking.atlasPath().string());
    walking.validateAtlas(walkingTexture.getSize());
    walkingTexture.setSmooth(false);
    if (idle.pivot() != walking.pivot() || idle.frameRect().size != walking.frameRect().size)
        throw std::runtime_error("Idle and Walking must share canvas and pivot");
    AnimationClip* activeClip = &idle;
    sf::Sprite playerSprite(idleTexture, idle.frameRect());
    playerSprite.setOrigin(idle.pivot());
    playerSprite.setScale({visualScale, visualScale});
    std::cout << "Idle loaded: " << idle.atlasPath() << " frames=" << idle.frames()
              << " fps=" << idle.fps() << " atlas=" << idleTexture.getSize().x
              << 'x' << idleTexture.getSize().y << std::endl;
    std::cout << "Walking loaded: " << walking.atlasPath() << " frames=" << walking.frames()
              << " fps=" << walking.fps() << " atlas=" << walkingTexture.getSize().x
              << 'x' << walkingTexture.getSize().y << std::endl;
    unsigned int idleFrameChanges = 0;
    unsigned int walkingFrameChanges = 0;
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
        AnimationClip* nextClip = player.state() == MovementState::Walking ? &walking : &idle;
        const bool changedClip = nextClip != activeClip;
        if (changedClip)
        {
            activeClip = nextClip;
            activeClip->reset();
            playerSprite.setTexture(activeClip == &walking ? walkingTexture : idleTexture);
        }
        const int previousFrame = activeClip->frameIndex();
        if (!changedClip)
            activeClip->advance(deltaTime);
        auto& frameChanges = activeClip == &walking ? walkingFrameChanges : idleFrameChanges;
        frameChanges += activeClip->frameIndex() != previousFrame;
        playerSprite.setTextureRect(activeClip->frameRect());
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
    std::cout << "Idle frame changes: " << idleFrameChanges << std::endl;
    std::cout << "Walking frame changes: " << walkingFrameChanges << std::endl;
}
catch (const std::exception& error)
{
    std::cerr << "Project Slit startup/runtime error: " << error.what() << std::endl;
    return 1;
}
