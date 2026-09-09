#include "Input.hpp"
#include "Player.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
bool near(float a, float b, float tolerance = 0.02f)
{
    return std::abs(a - b) < tolerance;
}
const std::vector<sf::FloatRect> floorOnly{{{0.f, 520.f}, {800.f, 80.f}}};
const std::vector<sf::FloatRect> room{
    {{0.f, 520.f}, {800.f, 80.f}}, {{40.f, 180.f}, {24.f, 340.f}},
    {{220.f, 440.f}, {160.f, 20.f}}, {{430.f, 360.f}, {140.f, 20.f}}
};
void advance(Player& player, const InputIntent& intent, int frames,
             const std::vector<sf::FloatRect>& solids = floorOnly, float dt = 1.f / 60.f)
{
    for (int i = 0; i < frames; ++i)
        player.update(intent, dt, solids, 800.f);
}
void noOverlap(const Player& player, const std::vector<sf::FloatRect>& solids)
{
    const auto p = player.collisionBounds();
    for (const auto& s : solids)
    {
        const float x = std::min(p.position.x + p.size.x, s.position.x + s.size.x) - std::max(p.position.x, s.position.x);
        const float y = std::min(p.position.y + p.size.y, s.position.y + s.size.y) - std::max(p.position.y, s.position.y);
        check(x <= 0.001f || y <= 0.001f, "solid penetration");
    }
}
void baseline()
{
    Player p;
    advance(p, {}, 60);
    check(p.grounded() && p.state() == MovementState::Grounded, "floor grounding");
    check(near(p.collisionBounds().position.y, 472.f), "floor position");
    advance(p, {{1.f, 0.f}}, 60);
    check(near(p.collisionBounds().position.x, 360.f), "delta time right movement");
    advance(p, {{-1.f, 0.f}}, 60);
    check(near(p.collisionBounds().position.x, 100.f), "left movement");
    p.update({{}, true}, 1.f / 60.f, floorOnly, 800.f);
    check(!p.grounded() && p.velocity().y < 0.f, "jump");
    advance(p, {}, 60);
    check(p.grounded() && near(p.velocity().y, 0.f), "gravity and landing");
    advance(p, {{-1.f, 0.f}}, 300);
    check(near(p.collisionBounds().position.x, 0.f), "left clamp");
    advance(p, {{1.f, 0.f}}, 300);
    check(near(p.collisionBounds().position.x, 768.f), "right clamp");
    for (const int rate : {30, 60, 144})
    {
        Player timed;
        advance(timed, {{1.f, 0.f}}, rate, floorOnly, 1.f / static_cast<float>(rate));
        check(near(timed.collisionBounds().position.x, 360.f, 0.1f), "frame rate independent movement");
    }
}
void terrain()
{
    Player landing({250.f, 300.f});
    advance(landing, {}, 60, room);
    check(landing.grounded() && near(landing.collisionBounds().position.y, 392.f), "platform top landing");
    Player underside({250.f, 472.f});
    underside.update({{}, true}, 0.05f, room, 800.f);
    check(underside.collisionBounds().position.y >= 460.f && underside.velocity().y >= 0.f, "platform underside blocks jump");
    Player side({170.f, 410.f});
    side.update({{1.f, 0.f}, false, {1.f, 0.f}}, 0.05f, room, 800.f);
    check(near(side.collisionBounds().position.x, 188.f), "platform side blocks dash");
    noOverlap(side, room);
    // Reach the first platform with an ordinary jump from the floor.
    Player reachable({170.f, 472.f});
    reachable.update({{1.f, 0.f}, true}, 1.f / 60.f, room, 800.f);
    advance(reachable, {{1.f, 0.f}}, 21, room);
    advance(reachable, {}, 30, room);
    check(reachable.grounded() && near(reachable.collisionBounds().position.y, 392.f), "platform reachable by jump");
    Player walkOff({350.f, 392.f});
    advance(walkOff, {{1.f, 0.f}}, 12, room);
    check(!walkOff.grounded(), "leaving platform clears grounded");
    const std::vector<sf::FloatRect> thin{{{0.f, 300.f}, {800.f, 1.f}}};
    Player fast({200.f, 240.f});
    fast.update({{}, false, {0.f, 1.f}}, 0.05f, thin, 800.f);
    check(fast.grounded() && near(fast.collisionBounds().position.y, 252.f), "thin platform blocks downward dash");
}
void dash()
{
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
        {
            if (x == 0 && y == 0) continue;
            Player p({400.f, 200.f});
            p.update({{}, false, {static_cast<float>(x), static_cast<float>(y)}}, 0.01f, floorOnly, 800.f);
            check(p.state() == MovementState::Dashing, "eight direction dash starts");
            const auto v = p.velocity();
            check(near(std::sqrt(v.x * v.x + v.y * v.y), Movement::dashSpeed), "diagonal normalized speed");
            check(v.x * x >= 0.f && v.y * y >= 0.f, "dash direction");
        }
    Player ground;
    ground.update({{}, false, {1.f, 0.f}}, 0.01f, floorOnly, 800.f);
    check(ground.state() == MovementState::Grounded, "ground dash rejected");
    Player p({400.f, 200.f});
    p.update({{}, false, {1.f, 0.f}}, 0.01f, floorOnly, 800.f);
    p.update({{}, false, {-1.f, 0.f}}, 0.01f, floorOnly, 800.f);
    check(p.velocity().x > 0.f, "dash cannot restart while active");
    advance(p, {}, 9);
    check(p.state() != MovementState::Dashing && !p.grounded(), "dash duration ends airborne");
    p.update({{}, false, {-1.f, 0.f}}, 0.01f, floorOnly, 800.f);
    check(p.state() == MovementState::Dashing && p.velocity().x < 0.f, "repeat airborne dash without landing");
}
void walls()
{
    Player p({100.f, 250.f});
    advance(p, {{-1.f, 0.f}}, 15, room);
    check(near(p.collisionBounds().position.x, 64.f) && p.touchingWall(), "left wall blocks movement");
    check(p.state() == MovementState::WallSliding && p.velocity().y <= Movement::wallSlideSpeed, "wall slide speed");
    const float before = p.collisionBounds().position.y;
    p.update({{}, true}, 1.f / 60.f, room, 800.f);
    check(p.velocity().y < 0.f && p.collisionBounds().position.y < before, "wall re-jump");
    advance(p, {}, 10, room);
    const float second = p.collisionBounds().position.y;
    p.update({{}, true}, 1.f / 60.f, room, 800.f);
    check(p.velocity().y < -490.f && p.collisionBounds().position.y < second, "repeated ninja jump");
    noOverlap(p, room);
    advance(p, {{1.f, 0.f}}, 10, room);
    check(!p.touchingWall(), "wall contact clears on separation");
    const float velocityBefore = p.velocity().y;
    p.update({{}, true}, 1.f / 60.f, room, 800.f);
    check(p.velocity().y > velocityBefore, "no free midair jump away from wall");
}
void inputs()
{
    using Key = sf::Keyboard::Key;
    for (const auto key : {Key::A, Key::Left, Key::D, Key::Right, Key::W, Key::Up, Key::S, Key::Down})
    {
        Input input;
        input.keyPressed(key, 1.f);
        const auto first = input.consume();
        check(first.dashDirection == sf::Vector2f{}, "first tap no dash");
        input.keyPressed(key, 1.05f);
        check(input.consume().dashDirection == sf::Vector2f{}, "key repeat no dash");
        input.keyReleased(key);
        input.keyPressed(key, 1.15f);
        check(input.consume().dashDirection == first.direction, "all key aliases double tap");
        check(input.consume().dashDirection == sf::Vector2f{}, "request consumed once");
    }
    for (const auto vertical : {Key::W, Key::S})
        for (const auto horizontal : {Key::A, Key::D})
        {
            Input input;
            input.keyPressed(vertical, 1.f);
            input.keyPressed(horizontal, 1.01f);
            input.keyReleased(horizontal);
            input.keyPressed(horizontal, 1.1f);
            const auto diagonal = input.consume().dashDirection;
            check(std::abs(diagonal.x) == 1.f && std::abs(diagonal.y) == 1.f, "diagonal combines current axes");
            input.keyReleased(vertical);
            check(input.consume().dashDirection == sf::Vector2f{}, "diagonal request not repeated");
        }
    Input input;
    input.keyPressed(Key::D, 1.f);
    input.keyPressed(Key::Right, 1.1f);
    check(input.consume().dashDirection == sf::Vector2f{}, "held aliases do not double tap");
    input.reset();
    input.keyPressed(Key::D, 2.f);
    input.keyReleased(Key::D);
    input.keyPressed(Key::D, 2.5f);
    check(input.consume().dashDirection == sf::Vector2f{}, "late second tap rejected");
    input.keyPressed(Key::A, 2.6f);
    check(input.consume().direction.x == 0.f, "opposing directions cancel");
    input.keyPressed(Key::Space, 3.f);
    check(input.consume().jumpRequested, "space jumps");
    input.keyPressed(Key::Space, 3.1f);
    check(!input.consume().jumpRequested, "held space does not jump repeatedly");
    input.reset();
    const auto reset = input.consume();
    check(reset.direction == sf::Vector2f{} && !reset.jumpRequested, "focus reset clears input");
    input.keyPressed(Key::D, 3.2f);
    check(input.consume().dashDirection == sf::Vector2f{}, "focus reset clears tap history");
}
}

int main()
{
    try
    {
        baseline(); terrain(); dash(); walls(); inputs();
        std::cout << "PASS: baseline, terrain, eight-way/repeated dash, wall jumps, input\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
