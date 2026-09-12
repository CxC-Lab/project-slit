#include "Camera.hpp"
#include "Background.hpp"
#include "Display.hpp"
#include "Input.hpp"
#include "AnimationClip.hpp"
#include "Player.hpp"
#include "TurnVisual.hpp"
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

int main(int argc, char** argv) try
{
    Display display;
    sf::RenderWindow window;
    display.open(window);
    sf::View camera(sf::FloatRect({0.f, 0.f}, Display::referenceViewSize));
    Display::apply(camera, window.getSize());

    if (argc > 2) throw std::runtime_error("Usage: project_slit.exe [practice_room|avatar_lake]");
    const auto regionFile = Region::findFile(argc == 2 ? argv[1] : "practice_room");
    const Region room(regionFile);
    const Background background(regionFile.parent_path().parent_path()/"backgrounds"/regionFile.filename());
    Player player(room.spawn());
    sf::RectangleShape playerShape(Movement::collisionSize);
    playerShape.setFillColor(sf::Color(240, 200, 100));
    constexpr bool showPlayerCollider = false;
    constexpr float visualScale = 0.5f; // Rendering-only playtest candidate.
    const auto manifest = AnimationClip::findManifest();
    ClipVisual idle(manifest, "Idle");
    ClipVisual walking(manifest, "Walking");
    ClipVisual sprinting(manifest, "Sprinting");
    ClipVisual turn(manifest, "Turn");
    ClipVisual jumping(manifest, "Jumping");
    ClipVisual falling(manifest, "Falling");
    ClipVisual fastFalling(manifest, "FastFalling");
    ClipVisual airDashing(manifest, "AirDashing");
    ClipVisual slowFalling(manifest, "SlowFalling");
    ClipVisual wallSliding(manifest, "WallSliding");
    ClipVisual landing(manifest, "Landing");
    bool landingPlaying = false;
    if (landing.clip.loops())
        throw std::runtime_error("Landing must be non-looping");
    bool jumpPlaying = false;
    MovementState previousMovementState = player.state();
    TurnVisual facing;
    if (turn.clip.loops())
        throw std::runtime_error("Turn must be a non-looping clip");
    if (jumping.clip.loops() || !falling.clip.loops())
        throw std::runtime_error("Jumping must be one-shot and Falling must loop");
    for (const auto* visual : {&walking, &sprinting, &turn, &jumping, &falling, &fastFalling, &airDashing, &slowFalling, &wallSliding, &landing})
        if (idle.clip.pivot() != visual->clip.pivot() || idle.clip.frameRect().size != visual->clip.frameRect().size)
            throw std::runtime_error("All eleven animation clips must share canvas and pivot");
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
                window.setMouseCursorVisible(true);
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
        const InputIntent intent = input.consume();
        player.update(intent, deltaTime, room.solids(), room.bounds().size.x);
        const auto playerBounds = player.collisionBounds();
        playerShape.setPosition(playerBounds.position);
        const auto movementState = player.state();
        const int previousJumpFrame = jumping.clip.frameIndex();
        if (movementState == MovementState::Jumping && previousMovementState != MovementState::Jumping)
        {
            jumping.clip.reset();
            jumpPlaying = true;
        }
        else if (jumpPlaying)
        {
            // Like Turn, finish the one-shot independently of subsequent physics states.
            jumping.clip.advance(deltaTime);
            jumpPlaying = !jumping.clip.finished();
        }
        previousMovementState = movementState;
        jumping.frameChanges += jumping.clip.frameIndex() != previousJumpFrame;
        ClipVisual* nextVisual = &idle;
        switch (movementState)
        {
        case MovementState::Walking: nextVisual = &walking; break;
        case MovementState::Sprinting: nextVisual = &sprinting; break;
        case MovementState::Jumping: nextVisual = &jumping; break;
        case MovementState::Falling: nextVisual = &falling; break;
        case MovementState::FastFalling: nextVisual = &fastFalling; break;
        case MovementState::AirDashing: nextVisual = &airDashing; break;
        case MovementState::SlowFalling: nextVisual = &slowFalling; break;
        case MovementState::WallSliding: nextVisual = &wallSliding; break;
        default: break;
        }
        if (jumpPlaying)
            nextVisual = &jumping;
        const int previousTurnFrame = turn.clip.frameIndex();
        facing.update(intent.direction.x, deltaTime, turn.clip);
        turn.frameChanges += turn.clip.frameIndex() != previousTurnFrame;
        if (player.landImpact())
        {
            landing.clip.reset();
            landingPlaying = true;
        }
        // Landing yields immediately to locomotion and existing one-shot overrides.
        if (movementState != MovementState::Idle || jumpPlaying || facing.turning)
            landingPlaying = false;
        if (landingPlaying)
        {
            const int previousLandingFrame = landing.clip.frameIndex();
            if (!player.landImpact())
                landing.clip.advance(deltaTime);
            landing.frameChanges += landing.clip.frameIndex() != previousLandingFrame;
            landingPlaying = !landing.clip.finished();
            if (landingPlaying)
                nextVisual = &landing;
        }
        if (facing.turning)
            nextVisual = &turn;
        const bool changedClip = nextVisual != activeVisual;
        if (changedClip)
        {
            activeVisual = nextVisual;
            if (activeVisual != &turn && activeVisual != &jumping && activeVisual != &landing) // One-shot timers are managed above.
                activeVisual->clip.reset();
            playerSprite.setTexture(activeVisual->texture);
        }
        const int previousFrame = activeVisual->clip.frameIndex();
        if (activeVisual != &turn && activeVisual != &jumping && activeVisual != &landing)
        {
            if (!changedClip)
                activeVisual->clip.advance(deltaTime);
            activeVisual->frameChanges += activeVisual->clip.frameIndex() != previousFrame;
        }
        playerSprite.setScale({facing.facingLeft ? -visualScale : visualScale, visualScale});
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
        background.render(window);
        room.render(window);
        window.draw(playerSprite);
        if (showPlayerCollider)
            window.draw(playerShape);
        window.display();
    }
    std::cout << "Animation frame changes: Idle=" << idle.frameChanges
              << " Walking=" << walking.frameChanges << " Sprinting=" << sprinting.frameChanges << " Turn=" << turn.frameChanges
              << " Jumping=" << jumping.frameChanges << " Falling=" << falling.frameChanges
              << " FastFalling=" << fastFalling.frameChanges << std::endl;
}
catch (const std::exception& error)
{
    std::cerr << "Project Slit startup/runtime error: " << error.what() << std::endl;
    return 1;
}
