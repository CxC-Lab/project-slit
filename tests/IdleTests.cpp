#include "AnimationClip.hpp"
#include "TurnVisual.hpp"
#include <SFML/Graphics/Transformable.hpp>

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
    AnimationClip sprinting(manifest, "Sprinting");
    sf::Image sprintingImage;
    check(sprintingImage.loadFromFile(sprinting.atlasPath()), "Sprinting atlas loads");
    sprinting.validateAtlas(sprintingImage.getSize());
    check(sprinting.frames() == 4 && sprinting.fps() == 8. && sprinting.loops(), "Sprinting manifest contract uses 8 fps");
    check(sprinting.pivot() == idle.pivot() && sprinting.frameRect().size == idle.frameRect().size,
          "Sprinting shares canvas and pivot");
    for (int i = 0; i < sprinting.frames(); ++i)
    {
        const auto rect = sprinting.frameRect();
        check(sprinting.frameIndex() == i && rect.position.x == i * rect.size.x && rect.position.y == 0,
              "Sprinting left-to-right frame slicing");
        check(rect.position.x + rect.size.x <= static_cast<int>(sprintingImage.getSize().x) &&
              rect.position.y + rect.size.y <= static_cast<int>(sprintingImage.getSize().y), "Sprinting rect in atlas");
        sprinting.advance(1. / sprinting.fps());
    }
    check(sprinting.frameIndex() == 0, "Sprinting loop wrap");
    sprinting.advance(0.125);
    check(sprinting.frameIndex() == 1, "Sprinting advances at 8 fps, not Idle 4 fps");
    sprinting.reset();
    check(sprinting.frameIndex() == 0, "Sprinting transition resets frame");
    AnimationClip turn(manifest, "Turn");
    sf::Image turnImage;
    check(turnImage.loadFromFile(turn.atlasPath()), "Turn atlas loads");
    turn.validateAtlas(turnImage.getSize());
    check(turn.frames() == 3 && turn.fps() == 10. && !turn.loops(), "Turn manifest contract");
    check(turn.pivot() == idle.pivot() && turn.frameRect().size == idle.frameRect().size, "Turn shared canvas and pivot");
    for (int i = 0; i < turn.frames(); ++i)
    {
        check(!turn.finished() && turn.frameIndex() == i, "Turn plays every frame before completion");
        const auto rect = turn.frameRect();
        check(rect.position.x == i * rect.size.x && rect.position.y == 0 &&
              rect.position.x + rect.size.x <= static_cast<int>(turnImage.getSize().x) &&
              rect.size.y <= static_cast<int>(turnImage.getSize().y), "Turn rect inside atlas");
        turn.advance(1. / turn.fps());
    }
    check(turn.finished() && turn.frameIndex() == 2, "Turn finishes and holds last frame after 0.3 seconds");
    turn.advance(1.);
    check(turn.finished() && turn.frameIndex() == 2, "non-looping Turn never wraps");
    turn.reset();
    check(!turn.finished() && turn.frameIndex() == 0 && !idle.finished(), "reset clears finish; loop clips never finish");
    TurnVisual facing;
    check(!facing.update(0.f, 0.01, turn) && !facing.facingLeft, "neutral preserves initial right facing");
    check(facing.update(-1.f, 0.01, turn) && facing.turning && facing.facingLeft, "right to left starts mirrored Turn");
    for (int i = 0; i < 5; ++i)
    {
        check(!facing.update(i % 2 ? -1.f : 1.f, 0.05, turn), "rapid reversal cannot restart active Turn");
        check(facing.facingLeft && !turn.finished(), "facing is locked during current Turn");
    }
    check(turn.frameIndex() == 2, "rapid reversal progresses beyond frame zero");
    check(facing.update(1.f, 0.051, turn) && !facing.facingLeft && facing.turning,
          "latest intent starts opposite Turn only after completion");
    facing.update(0.f, turn.frames() / turn.fps(), turn);
    check(!facing.turning && !facing.facingLeft, "neutral retains direction and completed override ends");
    sf::Transformable visual;
    visual.setOrigin(turn.pivot());
    visual.setPosition({123.f, 456.f});
    for (float sign : {-1.f, 1.f})
    {
        visual.setScale({sign * 0.5f, 0.5f});
        check(visual.getTransform().transformPoint(turn.pivot()) == visual.getPosition(), "flip leaves pivot at exact world anchor");
    }
    AnimationClip jumping(manifest, "Jumping");
    AnimationClip falling(manifest, "Falling");
    for (auto* clip : {&jumping, &falling})
    {
        sf::Image atlas;
        check(atlas.loadFromFile(clip->atlasPath()), "air animation atlas loads");
        clip->validateAtlas(atlas.getSize());
        check(clip->pivot() == idle.pivot() && clip->frameRect().size == idle.frameRect().size,
              "air animation canvas and pivot shared");
        for (int i = 0; i < clip->frames(); ++i)
        {
            const auto rect = clip->frameRect();
            check(clip->frameIndex() == i && rect.position.x == i * rect.size.x &&
                  rect.position.x + rect.size.x <= static_cast<int>(atlas.getSize().x) &&
                  rect.position.y + rect.size.y <= static_cast<int>(atlas.getSize().y), "air animation rect range");
            clip->advance(1. / clip->fps());
        }
    }
    check(!jumping.loops() && jumping.finished(), "Jumping one-shot completes");
    check(falling.loops() && !falling.finished() && falling.frameIndex() == 0, "Falling loops");
    jumping.reset();
    const double jumpDuration = jumping.frames() / jumping.fps();
    for (int i = 0; i < 5; ++i) jumping.advance(jumpDuration / 8.);
    check(!jumping.finished() && jumping.frameIndex() > 0, "Jumping progresses without per-frame reset");
    // Entering Falling before the visual completes must not reset or replace the timer.
    for (int i = 0; i < 3; ++i) jumping.advance(jumpDuration / 8.);
    check(jumping.finished(), "one-shot finishes across physics state changes");
    jumping.reset();
    check(!jumping.finished() && jumping.frameIndex() == 0, "Jumping re-entry restarts one-shot");
    jumping.advance(jumpDuration / 8.);
    check(jumping.frameIndex() == 0, "Jumping re-entry resets fractional timer");
    falling.reset();
    falling.advance(2. * falling.frames() / falling.fps());
    check(falling.frameIndex() == 0 && !falling.finished(), "Falling remains looping over multiple cycles");
    AnimationClip fastFalling(manifest, "FastFalling");
    sf::Image fastFallingImage;
    check(fastFallingImage.loadFromFile(fastFalling.atlasPath()), "FastFalling atlas loads");
    fastFalling.validateAtlas(fastFallingImage.getSize());
    check(fastFalling.loops(), "FastFalling loops from manifest");
    check(fastFalling.pivot() == idle.pivot() && fastFalling.frameRect().size == idle.frameRect().size,
          "FastFalling shares canvas and pivot");
    for (int i = 0; i < fastFalling.frames(); ++i)
    {
        const auto rect = fastFalling.frameRect();
        check(fastFalling.frameIndex() == i && rect.position.x == i * rect.size.x && rect.position.y == 0 &&
              rect.position.x + rect.size.x <= static_cast<int>(fastFallingImage.getSize().x) &&
              rect.size.y <= static_cast<int>(fastFallingImage.getSize().y), "FastFalling frame rect range");
        fastFalling.advance(1. / fastFalling.fps());
    }
    check(fastFalling.frameIndex() == 0 && !fastFalling.finished(), "FastFalling wraps and stays active");
    AnimationClip airDashing(manifest, "AirDashing");
    sf::Image airDashingImage;
    check(airDashingImage.loadFromFile(airDashing.atlasPath()), "AirDashing atlas loads");
    airDashing.validateAtlas(airDashingImage.getSize());
    check(!airDashing.loops(), "AirDashing is non-looping");
    check(airDashing.pivot() == idle.pivot() && airDashing.frameRect().size == idle.frameRect().size,
          "AirDashing shared canvas and pivot");
    for (int i = 0; i < airDashing.frames(); ++i)
    {
        const auto rect = airDashing.frameRect();
        check(airDashing.frameIndex() == i && rect.position.x == i * rect.size.x && rect.position.y == 0 &&
              rect.position.x + rect.size.x <= static_cast<int>(airDashingImage.getSize().x) &&
              rect.size.y <= static_cast<int>(airDashingImage.getSize().y), "AirDashing rect range");
        airDashing.advance(1. / airDashing.fps());
    }
    check(airDashing.finished(), "AirDashing completes without looping");
    std::cout << "PASS: manifest, atlas, paths, rectangles, timing and wrap\n";
}
catch (const std::exception& error)
{
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
}
