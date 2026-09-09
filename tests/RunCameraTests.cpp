#include "Camera.hpp"
#include "Input.hpp"
#include "Player.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
bool near(float a, float b, float tolerance = 0.05f) { return std::abs(a - b) < tolerance; }
const std::vector<sf::FloatRect> floorSolids{{{0.f, 520.f}, {3200.f, 80.f}}};
void step(Player& p, const InputIntent& intent, float dt = 1.f / 60.f)
{
    p.update(intent, dt, floorSolids, 3200.f);
}
void runs()
{
    using Key = sf::Keyboard::Key;
    for (const auto key : {Key::A, Key::Left, Key::D, Key::Right})
    {
        Input input;
        Player p({1600.f, 472.f});
        input.keyPressed(key, 1.f);
        step(p, input.consume());
        check(p.state() == MovementState::Grounded, "single tap walks");
        input.keyReleased(key);
        step(p, input.consume());
        input.keyPressed(key, 1.1f);
        step(p, input.consume());
        check(p.state() == MovementState::Running, "horizontal double tap starts run");
        check(std::abs(p.velocity().x) > 260.f && std::abs(p.velocity().x) < Movement::runMaxSpeed,
              "run accelerates rather than snapping");
        for (int i = 0; i < 60; ++i) step(p, input.consume());
        check(p.state() == MovementState::Running && near(std::abs(p.velocity().x), Movement::runMaxSpeed),
              "one-frame trigger sustains run to maximum");
        input.keyReleased(key);
        step(p, input.consume());
        check(p.state() == MovementState::Grounded && near(p.velocity().x, 0.f), "release stops run");
    }
    Player p;
    step(p, {{1.f, 0.f}, false, {1.f, 0.f}});
    step(p, {{-1.f, 0.f}});
    check(p.state() == MovementState::Grounded && near(p.velocity().x, -260.f), "reverse clears run without residual speed");
    step(p, {{1.f, 0.f}, false, {1.f, 0.f}});
    step(p, {{1.f, 0.f}, true});
    check(!p.grounded() && p.velocity().y < 0.f && near(p.velocity().x, 260.f), "run jump uses original air control");
    step(p, {{1.f, 0.f}, false, {1.f, 0.f}});
    check(p.state() == MovementState::Dashing, "same input in air starts dash");
    for (int i = 0; i < 120; ++i) step(p, {{1.f, 0.f}});
    check(p.state() == MovementState::Grounded && near(p.velocity().x, 260.f), "landing does not restore old run");
    Player vertical;
    step(vertical, {{0.f, -1.f}, false, {0.f, -1.f}});
    check(vertical.state() == MovementState::Grounded, "vertical double tap does not run");
    for (const int rate : {30, 60, 144})
    {
        Player timed;
        step(timed, {{1.f, 0.f}, false, {1.f, 0.f}}, 1.f / rate);
        for (int i = 1; i < rate; ++i) step(timed, {{1.f, 0.f}}, 1.f / rate);
        check(near(timed.velocity().x, 440.f), "run reaches same speed across frame rates");
        check(near(timed.collisionBounds().position.x, 513.f, 1.f), "run acceleration distance across frame rates");
    }
}
void cameras()
{
    const sf::FloatRect world({0.f, -1200.f}, {3200.f, 1800.f});
    sf::View view(sf::FloatRect({0.f, 0.f}, {800.f, 600.f}));
    for (float y : {496.f, 460.f, 420.f, 406.f, 420.f, 496.f})
        Camera::follow(view, {500.f, y}, world, 1.f / 60.f);
    check(near(view.getCenter().y, 300.f), "ordinary floor jump does not move camera");
    view.setCenter({500.f, -300.f});
    for (float y : {-200.f, -240.f, -290.f, -240.f, -200.f})
        Camera::follow(view, {500.f, y}, world, 1.f / 60.f);
    check(near(view.getCenter().y, -300.f), "ordinary elevated jump stays in dead-zone");
    Camera::follow(view, {700.f, -700.f}, world, 1.f / 60.f);
    check(view.getCenter().y < -300.f && view.getCenter().y > -580.f, "large ascent follows without snap");
    Camera::follow(view, {900.f, 400.f}, world, 1.f / 60.f);
    check(view.getCenter().y > -327.f, "large descent follows down");
    float reference = 0.f;
    for (const int rate : {30, 60, 144})
    {
        view.setCenter({400.f, 300.f});
        for (int i = 0; i < rate; ++i)
            Camera::follow(view, {1600.f, -800.f}, world, 1.f / rate);
        if (rate == 30) reference = view.getCenter().y;
        check(near(view.getCenter().y, reference, 0.01f), "exponential smoothing independent of frame rate");
        check(near(view.getCenter().x, 1600.f), "X direct follow preserved");
    }
    for (int i = 0; i < 300; ++i) Camera::follow(view, {-500.f, -5000.f}, world, 1.f / 60.f);
    check(near(view.getCenter().x, 400.f) && near(view.getCenter().y, -900.f), "left/top world clamps");
    for (int i = 0; i < 300; ++i) Camera::follow(view, {5000.f, 5000.f}, world, 1.f / 60.f);
    check(near(view.getCenter().x, 2800.f) && near(view.getCenter().y, 300.f), "right/bottom world clamps");
}
void climbingRoute()
{
    const std::vector<sf::FloatRect> solids{
        {{0.f, 520.f}, {3200.f, 80.f}}, {{40.f, 180.f}, {24.f, 340.f}},
        {{40.f, -1050.f}, {24.f, 1230.f}},
        {{128.f, 80.f}, {140.f, 20.f}}, {{128.f, -160.f}, {140.f, 20.f}},
        {{128.f, -400.f}, {140.f, 20.f}}, {{128.f, -640.f}, {140.f, 20.f}},
        {{128.f, -880.f}, {140.f, 20.f}}, {{128.f, -1080.f}, {140.f, 20.f}}
    };
    Player p;
    for (int i = 0; i < 20; ++i) p.update({{-1.f, 0.f}}, 1.f / 60.f, solids, 3200.f);
    check(p.touchingWall(), "climbing wall accessible from spawn");
    for (int i = 0; i < 240; ++i)
        p.update({{}, i % 10 == 0}, 1.f / 60.f, solids, 3200.f);
    check(p.collisionBounds().position.y < -1000.f, "wall seam permits repeated jumps into upper world");
    // A top-up dash and rightward movement reach the highest resting platform.
    p.update({{}, false, {0.f, -1.f}}, 0.05f, solids, 3200.f);
    for (int i = 0; i < 25; ++i) p.update({{1.f, 0.f}}, 1.f / 60.f, solids, 3200.f);
    for (int i = 0; i < 90; ++i) p.update({}, 1.f / 60.f, solids, 3200.f);
    check(p.grounded() && near(p.collisionBounds().position.y, -1128.f), "highest platform reachable and landable");
}
}
int main()
{
    try { runs(); cameras(); climbingRoute(); std::cout << "PASS: running, vertical camera, climbing route\n"; }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
